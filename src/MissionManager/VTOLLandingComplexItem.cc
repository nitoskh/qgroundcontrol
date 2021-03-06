/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VTOLLandingComplexItem.h"
#include "JsonHelper.h"
#include "MissionController.h"
#include "QGCGeo.h"
#include "SimpleMissionItem.h"
#include "PlanMasterController.h"
#include "FlightPathSegment.h"
#include "QGC.h"

#include <QPolygonF>

QGC_LOGGING_CATEGORY(VTOLLandingComplexItemLog, "VTOLLandingComplexItemLog")

const QString VTOLLandingComplexItem::name(tr("VTOL Landing"));

const char* VTOLLandingComplexItem::settingsGroup =            "VTOLLanding";
const char* VTOLLandingComplexItem::jsonComplexItemTypeValue = "vtolLandingPattern";

VTOLLandingComplexItem::VTOLLandingComplexItem(PlanMasterController* masterController, bool flyView, QObject* parent)
    : LandingComplexItem        (masterController, flyView, parent)
    , _metaDataMap              (FactMetaData::createMapFromJsonFile(QStringLiteral(":/json/VTOLLandingPattern.FactMetaData.json"), this))
    , _landingDistanceFact      (settingsGroup, _metaDataMap[finalApproachToLandDistanceName])
    , _finalApproachAltitudeFact(settingsGroup, _metaDataMap[finalApproachAltitudeName])
    , _loiterRadiusFact         (settingsGroup, _metaDataMap[loiterRadiusName])
    , _loiterClockwiseFact      (settingsGroup, _metaDataMap[loiterClockwiseName])
    , _landingHeadingFact       (settingsGroup, _metaDataMap[landingHeadingName])
    , _landingAltitudeFact      (settingsGroup, _metaDataMap[landingAltitudeName])
    , _useLoiterToAltFact       (settingsGroup, _metaDataMap[useLoiterToAltName])
    , _stopTakingPhotosFact     (settingsGroup, _metaDataMap[stopTakingPhotosName])
    , _stopTakingVideoFact      (settingsGroup, _metaDataMap[stopTakingVideoName])
{
    _editorQml      = "qrc:/qml/VTOLLandingPatternEditor.qml";
    _isIncomplete   = false;

    _init();

    // We adjust landing distance meta data to Plan View settings unless there was a custom build override
    if (QGC::fuzzyCompare(_landingDistanceFact.rawValue().toDouble(), _landingDistanceFact.rawDefaultValue().toDouble())) {
        Fact* vtolTransitionDistanceFact = qgcApp()->toolbox()->settingsManager()->planViewSettings()->vtolTransitionDistance();
        double vtolTransitionDistance = vtolTransitionDistanceFact->rawValue().toDouble();
        _landingDistanceFact.metaData()->setRawDefaultValue(vtolTransitionDistance);
        _landingDistanceFact.setRawValue(vtolTransitionDistance);
        _landingDistanceFact.metaData()->setRawMin(vtolTransitionDistanceFact->metaData()->rawMin());
    }

    _recalcFromHeadingAndDistanceChange();

    setDirty(false);
}

void VTOLLandingComplexItem::save(QJsonArray&  missionItems)
{
    QJsonObject saveObject = _save();

    saveObject[JsonHelper::jsonVersionKey] =                    1;
    saveObject[VisualMissionItem::jsonTypeKey] =                VisualMissionItem::jsonTypeComplexItemValue;
    saveObject[ComplexMissionItem::jsonComplexItemTypeKey] =    jsonComplexItemTypeValue;

    missionItems.append(saveObject);
}

bool VTOLLandingComplexItem::load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString)
{
    QList<JsonHelper::KeyValidateInfo> keyInfoList = {
        { JsonHelper::jsonVersionKey, QJsonValue::Double, true },
    };
    if (!JsonHelper::validateKeys(complexObject, keyInfoList, errorString)) {
        return false;
    }

    int version = complexObject[JsonHelper::jsonVersionKey].toInt();
    if (version != 1) {
        errorString = tr("%1 complex item version %2 not supported").arg(jsonComplexItemTypeValue).arg(version);
        _ignoreRecalcSignals = false;
        return false;
    }

    return _load(complexObject, sequenceNumber, jsonComplexItemTypeValue, false /* useDeprecatedRelAltKeys */, errorString);
}

MissionItem* VTOLLandingComplexItem::_createLandItem(int seqNum, bool altRel, double lat, double lon, double alt, QObject* parent)
{
    return new MissionItem(seqNum,
                           MAV_CMD_NAV_VTOL_LAND,
                           altRel ? MAV_FRAME_GLOBAL_RELATIVE_ALT : MAV_FRAME_GLOBAL,
                           0.0, 0.0, 0.0, 0.0,
                           lat, lon, alt,
                           true,                               // autoContinue
                           false,                              // isCurrentItem
                           parent);

}

void VTOLLandingComplexItem::_calcGlideSlope(void)
{
    // No glide slope calc for VTOL
}

bool VTOLLandingComplexItem::_isValidLandItem(const MissionItem& missionItem)
{
    if (missionItem.command() != MAV_CMD_NAV_LAND ||
            !(missionItem.frame() == MAV_FRAME_GLOBAL_RELATIVE_ALT || missionItem.frame() == MAV_FRAME_GLOBAL) ||
            missionItem.param1() != 0 || missionItem.param2() != 0 || missionItem.param3() != 0 || missionItem.param4() != 0) {
        return false;
    } else {
        return true;
    }
}

bool VTOLLandingComplexItem::scanForItem(QmlObjectListModel* visualItems, bool flyView, PlanMasterController* masterController)
{
    return _scanForItem(visualItems, flyView, masterController, _isValidLandItem, _createItem);
}
