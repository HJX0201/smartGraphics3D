#include "s_project_codec.h"

#include "s_kernel_shape_access.h"
#include "s_project_validation.h"
#include "s_version.h"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <cmath>
#include <sstream>

namespace smartGraphics3D
{
namespace
{
const QByteArray kMagic("SGRAPH3D");
constexpr quint32 kFormatVersion = 3;
constexpr quint64 kMaximumMetadataBytes = 64ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumShapeBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;

bool hasCurrentProjectExtension(const QString& file_path)
{
    const QString lower_path = file_path.toLower();
    return lower_path.endsWith(QStringLiteral(".sg3d")) ||
           lower_path.endsWith(QStringLiteral(".sg3d.autosave"));
}

QJsonArray vectorToJson(const QVector3D& value)
{
    return {value.x(), value.y(), value.z()};
}

QJsonArray matrixToJson(const QMatrix4x4& matrix)
{
    QJsonArray values;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            values.push_back(matrix(row, column));
        }
    }
    return values;
}

QMatrix4x4 matrixFromJson(const QJsonValue& value)
{
    const QJsonArray values = value.toArray();
    QMatrix4x4 matrix;
    if (values.size() != 16)
    {
        return matrix;
    }
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            matrix(row, column) = static_cast<float>(values.at(row * 4 + column).toDouble());
        }
    }
    return matrix;
}

QJsonObject coordinateSystemToJson(const SCoordinateSystem& coordinate_system)
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), coordinate_system.id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("name"), coordinate_system.name);
    json.insert(QStringLiteral("parentId"),
                coordinate_system.parent_id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("transform"), matrixToJson(coordinate_system.transform_to_parent));
    json.insert(QStringLiteral("source"), coordinate_system.source);
    json.insert(QStringLiteral("calibratedAt"),
                coordinate_system.calibrated_at.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("error"), coordinate_system.error);
    json.insert(QStringLiteral("valid"), coordinate_system.valid);
    return json;
}

SCoordinateSystem coordinateSystemFromJson(const QJsonObject& json)
{
    SCoordinateSystem coordinate_system;
    coordinate_system.id = QUuid(json.value(QStringLiteral("id")).toString());
    coordinate_system.name = json.value(QStringLiteral("name")).toString();
    coordinate_system.parent_id = QUuid(json.value(QStringLiteral("parentId")).toString());
    coordinate_system.transform_to_parent = matrixFromJson(json.value(QStringLiteral("transform")));
    coordinate_system.source = json.value(QStringLiteral("source")).toString();
    coordinate_system.calibrated_at = QDateTime::fromString(
        json.value(QStringLiteral("calibratedAt")).toString(), Qt::ISODateWithMs);
    coordinate_system.error = json.value(QStringLiteral("error")).toDouble();
    coordinate_system.valid = json.value(QStringLiteral("valid")).toBool(true);
    return coordinate_system;
}

QVector3D vectorFromJson(const QJsonValue& value)
{
    const QJsonArray array = value.toArray();
    return array.size() == 3
               ? QVector3D(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble())
               : QVector3D();
}

QJsonObject appearanceStyleToJson(const SAppearanceStyle& style)
{
    QJsonObject json;
    json.insert(QStringLiteral("color"), style.color.name(QColor::HexRgb));
    json.insert(QStringLiteral("transparency"), style.transparency);
    return json;
}

SAppearanceStyle appearanceStyleFromJson(const QJsonValue& value)
{
    const QJsonObject json = value.toObject();
    SAppearanceStyle style;
    style.color = QColor(json.value(QStringLiteral("color")).toString());
    style.transparency = json.value(QStringLiteral("transparency")).toDouble();
    return style;
}

QJsonObject importedAppearanceToJson(const SImportedAppearance& appearance)
{
    QJsonObject json;
    json.insert(QStringLiteral("base"), appearanceStyleToJson(appearance.base_style));
    json.insert(QStringLiteral("fallback"), appearanceStyleToJson(appearance.fallback_style));
    QJsonArray faces;
    for (const SFaceAppearance& face : appearance.face_overrides)
    {
        QJsonObject face_json = appearanceStyleToJson(face.style);
        face_json.insert(QStringLiteral("faceIndex"), face.face_index);
        faces.push_back(face_json);
    }
    json.insert(QStringLiteral("faces"), faces);
    return json;
}

SImportedAppearance importedAppearanceFromJson(const QJsonValue& value)
{
    SImportedAppearance appearance;
    if (!value.isObject())
    {
        return appearance;
    }
    const QJsonObject json = value.toObject();
    appearance.valid = true;
    appearance.base_style = appearanceStyleFromJson(json.value(QStringLiteral("base")));
    appearance.fallback_style = appearanceStyleFromJson(json.value(QStringLiteral("fallback")));
    for (const QJsonValue face_value : json.value(QStringLiteral("faces")).toArray())
    {
        const QJsonObject face_json = face_value.toObject();
        SFaceAppearance face;
        face.face_index = face_json.value(QStringLiteral("faceIndex")).toInt();
        face.style = appearanceStyleFromJson(face_json);
        appearance.face_overrides.push_back(std::move(face));
    }
    return appearance;
}

QJsonObject objectToJson(const SSceneObject& object, quint64 shape_size)
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), object.id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("parentId"), object.parent_id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("coordinateSystemId"),
                object.coordinate_system_id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("name"), object.name);
    json.insert(QStringLiteral("type"), static_cast<int>(object.type));
    json.insert(QStringLiteral("stage"), static_cast<int>(object.stage));
    json.insert(QStringLiteral("source"), object.source);
    json.insert(QStringLiteral("externalPath"), object.external_path);
    json.insert(QStringLiteral("visible"), object.visible);
    json.insert(QStringLiteral("locked"), object.locked);
    json.insert(QStringLiteral("frozen"), object.frozen);
    json.insert(QStringLiteral("externalReference"), object.external_reference);
    json.insert(QStringLiteral("qualityWarning"), object.quality_warning);
    json.insert(QStringLiteral("qualityMessage"), object.quality_message);
    json.insert(QStringLiteral("dataVersion"), object.data_version);
    json.insert(QStringLiteral("createdAt"), object.created_at.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("modifiedAt"), object.modified_at.toString(Qt::ISODateWithMs));
    json.insert(QStringLiteral("color"), object.display.color.name(QColor::HexArgb));
    json.insert(QStringLiteral("transparency"), object.display.transparency);
    json.insert(QStringLiteral("displayMode"), static_cast<int>(object.display.mode));
    if (object.imported_appearance.valid)
    {
        json.insert(QStringLiteral("importedAppearance"),
                    importedAppearanceToJson(object.imported_appearance));
        json.insert(QStringLiteral("useImportedAppearance"), object.use_imported_appearance);
    }
    json.insert(QStringLiteral("custom"), object.custom_properties);
    json.insert(QStringLiteral("transform"), matrixToJson(object.transform));
    json.insert(QStringLiteral("presentationGroupId"),
                object.presentation_group_id.toString(QUuid::WithoutBraces));
    json.insert(QStringLiteral("shapeSize"), static_cast<double>(shape_size));

    QJsonArray derived;
    for (const SObjectId& id : object.derived_from)
    {
        derived.push_back(id.toString(QUuid::WithoutBraces));
    }
    json.insert(QStringLiteral("derivedFrom"), derived);
    return json;
}

SSceneObject objectFromJson(const QJsonObject& json)
{
    SSceneObject object;
    object.id = QUuid(json.value(QStringLiteral("id")).toString());
    object.parent_id = QUuid(json.value(QStringLiteral("parentId")).toString());
    object.coordinate_system_id =
        QUuid(json.value(QStringLiteral("coordinateSystemId")).toString());
    object.name = json.value(QStringLiteral("name")).toString();
    object.type = static_cast<SObjectType>(json.value(QStringLiteral("type")).toInt());
    object.stage = static_cast<SDataStage>(json.value(QStringLiteral("stage")).toInt());
    object.source = json.value(QStringLiteral("source")).toString();
    object.external_path = json.value(QStringLiteral("externalPath")).toString();
    object.visible = json.value(QStringLiteral("visible")).toBool(true);
    object.locked = json.value(QStringLiteral("locked")).toBool(false);
    object.frozen = json.value(QStringLiteral("frozen")).toBool(false);
    object.external_reference = json.value(QStringLiteral("externalReference")).toBool(false);
    object.quality_warning = json.value(QStringLiteral("qualityWarning")).toBool(false);
    object.quality_message = json.value(QStringLiteral("qualityMessage")).toString();
    object.data_version = json.value(QStringLiteral("dataVersion")).toInt(1);
    object.created_at = QDateTime::fromString(json.value(QStringLiteral("createdAt")).toString(),
                                              Qt::ISODateWithMs);
    object.modified_at = QDateTime::fromString(json.value(QStringLiteral("modifiedAt")).toString(),
                                               Qt::ISODateWithMs);
    object.display.color = QColor(json.value(QStringLiteral("color")).toString());
    object.display.transparency = json.value(QStringLiteral("transparency")).toDouble();
    object.display.mode =
        static_cast<SDisplayMode>(json.value(QStringLiteral("displayMode")).toInt());
    object.imported_appearance =
        importedAppearanceFromJson(json.value(QStringLiteral("importedAppearance")));
    object.use_imported_appearance =
        json.value(QStringLiteral("useImportedAppearance")).toBool(false);
    object.custom_properties = json.value(QStringLiteral("custom")).toObject();
    object.transform = matrixFromJson(json.value(QStringLiteral("transform")));
    const QUuid presentation_group_id(json.value(QStringLiteral("presentationGroupId")).toString());
    if (!presentation_group_id.isNull())
    {
        object.presentation_group_id = presentation_group_id;
    }
    for (const QJsonValue& id : json.value(QStringLiteral("derivedFrom")).toArray())
    {
        object.derived_from.push_back(QUuid(id.toString()));
    }
    return object;
}

SResult<QByteArray> serializeShape(const SKernelShape& shape)
{
    if (shape.isNull())
    {
        return SResult<QByteArray>::success({});
    }
    try
    {
        std::ostringstream stream(std::ios::binary);
        BRepTools::Write(SKernelShapeAccess::native(shape), stream);
        const std::string data = stream.str();
        return SResult<QByteArray>::success(QByteArray(data.data(), static_cast<int>(data.size())));
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<QByteArray>::failure(SErrorCode::FileFailure, QObject::tr("几何序列化失败"),
                                            QString::fromLocal8Bit(failure.GetMessageString()));
    }
}

SResult<SKernelShape> deserializeShape(const QByteArray& data)
{
    if (data.isEmpty())
    {
        return SResult<SKernelShape>::success(SKernelShape());
    }
    try
    {
        std::istringstream stream(
            std::string(data.constData(), static_cast<std::size_t>(data.size())), std::ios::binary);
        TopoDS_Shape shape;
        BRep_Builder builder;
        BRepTools::Read(shape, stream, builder);
        if (shape.IsNull())
        {
            return SResult<SKernelShape>::failure(SErrorCode::CorruptData,
                                                  QObject::tr("项目中的几何数据已损坏"));
        }
        return SResult<SKernelShape>::success(SKernelShapeAccess::fromNative(shape));
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<SKernelShape>::failure(SErrorCode::CorruptData,
                                              QObject::tr("项目中的几何数据无法读取"),
                                              QString::fromLocal8Bit(failure.GetMessageString()));
    }
}
} // namespace

QStringList SProjectCodec::extensions() const
{
    return {QStringLiteral("sg3d")};
}

SResult<void> SProjectCodec::loadProject(S3dDocument& document, const QString& file_path) const
{
    return load(document, file_path);
}

SResult<void> SProjectCodec::saveProject(const S3dDocument& document,
                                         const QString& file_path) const
{
    return save(document, file_path);
}

SResult<void> SProjectCodec::save(const S3dDocument& document, const QString& file_path) const
{
    if (!hasCurrentProjectExtension(file_path))
    {
        return SResult<void>::failure(SErrorCode::Unsupported,
                                      QObject::tr("项目文件必须使用 .sg3d 扩展名"));
    }
    QJsonObject root;
    root.insert(QStringLiteral("projectId"), document.projectId().toString(QUuid::WithoutBraces));
    root.insert(QStringLiteral("projectName"), document.projectName());
    root.insert(QStringLiteral("lengthUnit"), static_cast<int>(document.unitSystem().lengthUnit()));
    root.insert(QStringLiteral("angleUnit"), static_cast<int>(document.unitSystem().angleUnit()));
    root.insert(QStringLiteral("applicationVersion"), QString::fromLatin1(SMARTGRAPHICS3D_VERSION));
    root.insert(QStringLiteral("savedAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonArray coordinate_systems_json;
    for (const SCoordinateSystem& coordinate_system : document.coordinateSystems())
    {
        coordinate_systems_json.push_back(coordinateSystemToJson(coordinate_system));
    }
    root.insert(QStringLiteral("coordinateSystems"), coordinate_systems_json);

    QJsonArray objects_json;
    QList<QByteArray> shape_data;
    for (const SSceneObject& object : document.objects())
    {
        const auto serialized = serializeShape(object.shape);
        if (!serialized)
        {
            return SResult<void>::failure(serialized.errorCode(), serialized.message(),
                                          serialized.details());
        }
        shape_data.push_back(serialized.value());
        objects_json.push_back(objectToJson(object, serialized.value().size()));
    }
    root.insert(QStringLiteral("objects"), objects_json);

    QJsonArray history_json;
    for (const SOperationRecord& record : document.history())
    {
        QJsonObject item;
        item.insert(QStringLiteral("id"), record.id.toString(QUuid::WithoutBraces));
        item.insert(QStringLiteral("name"), record.name);
        item.insert(QStringLiteral("parameters"), record.parameter_summary);
        item.insert(QStringLiteral("timestamp"), record.timestamp.toString(Qt::ISODateWithMs));
        item.insert(QStringLiteral("success"), record.success);
        item.insert(QStringLiteral("undoable"), record.undoable);
        QJsonArray object_ids;
        for (const SObjectId& id : record.object_ids)
        {
            object_ids.push_back(id.toString(QUuid::WithoutBraces));
        }
        item.insert(QStringLiteral("objectIds"), object_ids);
        history_json.push_back(item);
    }
    root.insert(QStringLiteral("history"), history_json);

    QJsonArray snapshots_json;
    for (const SSnapshotRecord& snapshot : document.snapshots())
    {
        QJsonObject snapshot_json;
        snapshot_json.insert(QStringLiteral("name"), snapshot.name);
        snapshot_json.insert(QStringLiteral("createdAt"),
                             snapshot.created_at.toString(Qt::ISODateWithMs));
        snapshot_json.insert(QStringLiteral("projectName"), snapshot.project_name);
        snapshot_json.insert(QStringLiteral("lengthUnit"),
                             static_cast<int>(snapshot.units.lengthUnit()));
        snapshot_json.insert(QStringLiteral("angleUnit"),
                             static_cast<int>(snapshot.units.angleUnit()));

        QJsonArray snapshot_coordinate_systems;
        for (const SCoordinateSystem& coordinate_system : snapshot.coordinate_systems)
        {
            snapshot_coordinate_systems.push_back(coordinateSystemToJson(coordinate_system));
        }
        snapshot_json.insert(QStringLiteral("coordinateSystems"), snapshot_coordinate_systems);

        QJsonArray snapshot_objects;
        for (const SSceneObject& object : snapshot.objects)
        {
            const auto serialized = serializeShape(object.shape);
            if (!serialized)
            {
                return SResult<void>::failure(serialized.errorCode(), serialized.message(),
                                              serialized.details());
            }
            shape_data.push_back(serialized.value());
            snapshot_objects.push_back(objectToJson(object, serialized.value().size()));
        }
        snapshot_json.insert(QStringLiteral("objects"), snapshot_objects);
        snapshots_json.push_back(snapshot_json);
    }
    root.insert(QStringLiteral("snapshots"), snapshots_json);

    const QByteArray metadata = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("无法创建项目文件"),
                                      file.errorString());
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << kMagic << kFormatVersion << metadata;
    for (const QByteArray& shape : shape_data)
    {
        stream << shape;
    }
    if (stream.status() != QDataStream::Ok || !file.commit())
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("项目保存失败"),
                                      file.errorString());
    }
    return SResult<void>::success();
}

SResult<void> SProjectCodec::load(S3dDocument& document, const QString& file_path) const
{
    if (!hasCurrentProjectExtension(file_path))
    {
        return SResult<void>::failure(SErrorCode::Unsupported,
                                      QObject::tr("不支持该项目文件扩展名"));
    }
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("无法打开项目文件"),
                                      file.errorString());
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    QByteArray magic;
    quint32 version = 0;
    QByteArray metadata;
    stream >> magic >> version >> metadata;
    if (magic != kMagic)
    {
        return SResult<void>::failure(SErrorCode::CorruptData,
                                      QObject::tr("不是有效的 smartGraphics3D 项目"));
    }
    if (version != kFormatVersion)
    {
        return SResult<void>::failure(
            SErrorCode::VersionMismatch, QObject::tr("项目版本不受支持"),
            QObject::tr("文件版本为 %1，当前支持版本为 %2。").arg(version).arg(kFormatVersion));
    }
    if (static_cast<quint64>(metadata.size()) > kMaximumMetadataBytes)
    {
        return SResult<void>::failure(SErrorCode::CorruptData, QObject::tr("项目元数据异常过大"));
    }

    QJsonParseError parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(metadata, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !json_document.isObject())
    {
        return SResult<void>::failure(SErrorCode::CorruptData, QObject::tr("项目元数据已损坏"),
                                      parse_error.errorString());
    }
    const QJsonObject root = json_document.object();
    const QUuid project_id(root.value(QStringLiteral("projectId")).toString());
    const QString project_name = root.value(QStringLiteral("projectName")).toString().trimmed();
    const int length_unit = root.value(QStringLiteral("lengthUnit")).toInt(-1);
    const int angle_unit = root.value(QStringLiteral("angleUnit")).toInt(-1);
    if (project_id.isNull() || project_name.isEmpty() ||
        !projectValidation::isValidLengthUnit(length_unit) ||
        !projectValidation::isValidAngleUnit(angle_unit))
    {
        return SResult<void>::failure(SErrorCode::CorruptData, QObject::tr("项目基本信息无效"));
    }
    std::vector<SSceneObject> objects;
    QHash<QUuid, QByteArray> presentation_group_data;
    QHash<QUuid, SKernelShape> presentation_group_shapes;
    for (const QJsonValue& value : root.value(QStringLiteral("objects")).toArray())
    {
        const QJsonObject object_json = value.toObject();
        const quint64 expected_size =
            static_cast<quint64>(object_json.value(QStringLiteral("shapeSize")).toDouble());
        if (expected_size > kMaximumShapeBytes)
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目中的几何数据异常过大"));
        }
        QByteArray shape_data;
        stream >> shape_data;
        if (stream.status() != QDataStream::Ok ||
            static_cast<quint64>(shape_data.size()) != expected_size)
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目文件被截断或几何长度不匹配"));
        }
        SSceneObject object = objectFromJson(object_json);
        const auto existing_group_data =
            presentation_group_data.constFind(object.presentation_group_id);
        if (existing_group_data != presentation_group_data.cend() &&
            existing_group_data.value() != shape_data)
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("共享显示组包含不一致的基础几何"));
        }
        if (existing_group_data != presentation_group_data.cend())
        {
            object.shape = presentation_group_shapes.value(object.presentation_group_id);
        }
        else
        {
            const auto shape = deserializeShape(shape_data);
            if (!shape)
            {
                return SResult<void>::failure(shape.errorCode(), shape.message(), shape.details());
            }
            object.shape = shape.value();
            presentation_group_data.insert(object.presentation_group_id, shape_data);
            presentation_group_shapes.insert(object.presentation_group_id, shape.value());
        }
        if (object.external_reference && !object.external_path.isEmpty() &&
            !QFileInfo::exists(object.external_path))
        {
            object.quality_warning = true;
            object.quality_message = QObject::tr("外部引用文件不存在，当前显示项目内的缓存几何");
        }
        objects.push_back(std::move(object));
    }

    QList<SOperationRecord> history;
    for (const QJsonValue& value : root.value(QStringLiteral("history")).toArray())
    {
        const QJsonObject item = value.toObject();
        SOperationRecord record;
        record.id = QUuid(item.value(QStringLiteral("id")).toString());
        record.name = item.value(QStringLiteral("name")).toString();
        record.parameter_summary = item.value(QStringLiteral("parameters")).toString();
        record.timestamp = QDateTime::fromString(item.value(QStringLiteral("timestamp")).toString(),
                                                 Qt::ISODateWithMs);
        record.success = item.value(QStringLiteral("success")).toBool();
        record.undoable = item.value(QStringLiteral("undoable")).toBool();
        for (const QJsonValue& id : item.value(QStringLiteral("objectIds")).toArray())
        {
            record.object_ids.push_back(QUuid(id.toString()));
        }
        history.push_back(std::move(record));
    }

    std::vector<SCoordinateSystem> coordinate_systems;
    for (const QJsonValue& value : root.value(QStringLiteral("coordinateSystems")).toArray())
    {
        coordinate_systems.push_back(coordinateSystemFromJson(value.toObject()));
    }
    const auto coordinate_validation =
        projectValidation::validateCoordinateSystems(coordinate_systems);
    if (!coordinate_validation)
    {
        return coordinate_validation;
    }
    const auto object_validation = projectValidation::validateObjects(objects, coordinate_systems);
    if (!object_validation)
    {
        return object_validation;
    }

    QList<SSnapshotRecord> snapshots;
    QSet<QString> snapshot_names;
    for (const QJsonValue& snapshot_value : root.value(QStringLiteral("snapshots")).toArray())
    {
        const QJsonObject snapshot_json = snapshot_value.toObject();
        SSnapshotRecord snapshot;
        QHash<QUuid, QByteArray> snapshot_group_data;
        QHash<QUuid, SKernelShape> snapshot_group_shapes;
        snapshot.name = snapshot_json.value(QStringLiteral("name")).toString();
        snapshot.created_at = QDateTime::fromString(
            snapshot_json.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
        snapshot.project_name = snapshot_json.value(QStringLiteral("projectName")).toString();
        snapshot.units.setLengthUnit(
            static_cast<SLengthUnit>(snapshot_json.value(QStringLiteral("lengthUnit")).toInt()));
        snapshot.units.setAngleUnit(
            static_cast<SAngleUnit>(snapshot_json.value(QStringLiteral("angleUnit")).toInt()));
        for (const QJsonValue& coordinate_value :
             snapshot_json.value(QStringLiteral("coordinateSystems")).toArray())
        {
            snapshot.coordinate_systems.push_back(
                coordinateSystemFromJson(coordinate_value.toObject()));
        }
        for (const QJsonValue& object_value :
             snapshot_json.value(QStringLiteral("objects")).toArray())
        {
            const QJsonObject object_json = object_value.toObject();
            const quint64 expected_size =
                static_cast<quint64>(object_json.value(QStringLiteral("shapeSize")).toDouble());
            if (expected_size > kMaximumShapeBytes)
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("快照中的几何数据异常过大"));
            }
            QByteArray shape_data;
            stream >> shape_data;
            if (stream.status() != QDataStream::Ok ||
                static_cast<quint64>(shape_data.size()) != expected_size)
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("项目快照被截断或几何长度不匹配"));
            }
            SSceneObject object = objectFromJson(object_json);
            const auto existing_group_data =
                snapshot_group_data.constFind(object.presentation_group_id);
            if (existing_group_data != snapshot_group_data.cend() &&
                existing_group_data.value() != shape_data)
            {
                return SResult<void>::failure(
                    SErrorCode::CorruptData,
                    QObject::tr("项目快照的共享显示组包含不一致的基础几何"));
            }
            if (existing_group_data != snapshot_group_data.cend())
            {
                object.shape = snapshot_group_shapes.value(object.presentation_group_id);
            }
            else
            {
                const auto shape = deserializeShape(shape_data);
                if (!shape)
                {
                    return SResult<void>::failure(shape.errorCode(), shape.message(),
                                                  shape.details());
                }
                object.shape = shape.value();
                snapshot_group_data.insert(object.presentation_group_id, shape_data);
                snapshot_group_shapes.insert(object.presentation_group_id, shape.value());
            }
            snapshot.objects.push_back(std::move(object));
        }
        if (snapshot.name.trimmed().isEmpty() ||
            !projectValidation::isValidLengthUnit(
                snapshot_json.value(QStringLiteral("lengthUnit")).toInt(-1)) ||
            !projectValidation::isValidAngleUnit(
                snapshot_json.value(QStringLiteral("angleUnit")).toInt(-1)))
        {
            return SResult<void>::failure(SErrorCode::CorruptData, QObject::tr("项目包含无效快照"));
        }
        if (snapshot_names.contains(snapshot.name))
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目包含重复快照名称"));
        }
        snapshot_names.insert(snapshot.name);
        const auto snapshot_coordinate_validation =
            projectValidation::validateCoordinateSystems(snapshot.coordinate_systems);
        if (!snapshot_coordinate_validation)
        {
            return snapshot_coordinate_validation;
        }
        const auto snapshot_object_validation =
            projectValidation::validateObjects(snapshot.objects, snapshot.coordinate_systems);
        if (!snapshot_object_validation)
        {
            return snapshot_object_validation;
        }
        snapshots.push_back(std::move(snapshot));
    }

    SUnitSystem units;
    units.setLengthUnit(static_cast<SLengthUnit>(length_unit));
    units.setAngleUnit(static_cast<SAngleUnit>(angle_unit));
    document.replaceAll(project_id, project_name, std::move(objects), std::move(history), units,
                        std::move(coordinate_systems), std::move(snapshots));
    document.setFilePath(QFileInfo(file_path).absoluteFilePath());
    return SResult<void>::success();
}
} // namespace smartGraphics3D
