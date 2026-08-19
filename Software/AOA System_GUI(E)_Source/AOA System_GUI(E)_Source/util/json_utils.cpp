
#include "RTLSDisplayApplication.h"
#include "RTLSClient.h"
#include <json_utils.h>

typedef struct
{
    int                 slot;
    unsigned long long  addr64;
    unsigned short      addr16;
    unsigned short      multFast;
    unsigned short      multSlow;
    unsigned short      mode;

    struct{
        uint16_t    rangeNum;           //number from Tag Poll and Final messages indicates the current range number
        uint32_t    resTime_us;         //reseption time of Final wrt to SuperFrame start
        double      pdoa_deg;           //phase differences in degrees
        double      dist_m;             //distance, m
        double      xdist_m;            //X distance, m
        double      ydist_m;            //Y distance, m
        double      clockOffset_ppm;    //clock offset in ppm
        uint16_t    vData;              //service message data from the tag: (stationary, etc)
        int         accX;               //accelerometer X data
        int         accY;               //accelerometer Y data
        int         accZ;               //accelerometer Z data

    }twr;
}tag_data_t;

typedef struct
{
    short      antennaTX_A;   //in DW time units
    short      antennaRX_A;   //in DW time units
    short      antennaTX_B;   //in DW time units
    short      antennaRX_B;   //in DW time units
    short      pdoaOffset;    //in milli radians
    short      distOffset;    //in centimeters
    short      accThreshold;               // in milli g
    short      accStationaryDuration;      // in ms (time to decide that node is stationary when below threshold)
    short      accMovingDuration;      // in ms (time to decide that node is moving when above threshold)

}calib_data_t;

void fromTwrObjToTagData(QJsonObject *pJobj, tag_data_t* tag)
{
    bool ok;
    tag->addr16  = pJobj->value("a16").toString().toShort(&ok, 16);

    tag->twr.rangeNum = pJobj->value("R").toInt();
    tag->twr.resTime_us= pJobj->value("T").toInt();
    tag->twr.clockOffset_ppm= pJobj->value("O").toDouble() /100; //Node reporting CO w.r.t. tag in 1/100 ppm
    tag->twr.dist_m= pJobj->value("D").toDouble() /100;          //Node reporting range to tag in cm
    tag->twr.pdoa_deg = pJobj->value("P").toDouble();            //Node reporting pdoa in degrees
    tag->twr.xdist_m= pJobj->value("Xcm").toDouble() /100;        //X direction w.r.t. node, cm
    tag->twr.ydist_m= pJobj->value("Ycm").toDouble() /100;        //Y direction w.r.t. node, cm
    tag->twr.vData = pJobj->value("V").toInt();
    tag->twr.accX = pJobj->value("X").toInt();
    tag->twr.accY = pJobj->value("Y").toInt();
    tag->twr.accZ = pJobj->value("Z").toInt();
}

void fromObjToCalibData(QJsonObject *pJobj, calib_data_t* calib)
{

    calib->antennaTX_A = pJobj->value("ANTTXA").toInt();
    calib->antennaRX_A = pJobj->value("ANTRXA").toInt();
    calib->antennaTX_B = pJobj->value("ANTTXB").toInt();
    calib->antennaRX_B = pJobj->value("ANTRXB").toInt();

    calib->pdoaOffset = pJobj->value("PDOAOFF").toInt();
    calib->distOffset = pJobj->value("RNGOFF").toInt();

    calib->accThreshold = pJobj->value("ACCTHR").toInt();
    calib->accStationaryDuration = pJobj->value("ACCSTAT").toInt();
    calib->accMovingDuration = pJobj->value("ACCMOVE").toInt();
}


void fromObjToTagData(QJsonObject *pJobj, tag_data_t* tag)
{
    bool ok;
    tag->slot    = pJobj->value("slot").toString().toInt(&ok, 16);
    tag->addr64  = pJobj->value("a64").toString().toULongLong(&ok, 16);
    tag->addr16  = pJobj->value("a16").toString().toShort(&ok, 16);
    tag->multFast= pJobj->value("F").toString().toInt(&ok, 16);
    tag->multSlow= pJobj->value("S").toString().toInt(&ok, 16);
    tag->mode    = pJobj->value("M").toString().toInt(&ok, 16);

    fromTwrObjToTagData(pJobj, tag); //check are there any other tag attributes present
}


int check_json_stream(const QByteArray st)
{
/* JSON reporting:
 *
 * Root elements:
 * "NewTag": string (addr64)           | on discovering of new Tag
 * "DList" : array of strings (addr64) | reply to getDlist
 * "KList" : array of tag objects      | reply to getKlist
 * "TagAdded"   : tag object           | reply to Add2List
 * "TagDeleted" : string (addr64)      | reply to delTag
 * "TWR" : twr object                  | on calculation of new range
 * "SN"  : service object(accel)       | on receiving data from IMU sensor
 *
 * 'JSxxxx{"NewTag":
 *             <string>//address64, string
 *        }'
    //Example of NewTagstring:
    //-
 *
 * 'JSxxxx{"DList": ["addr64_1","addr64_2","addr64_3"]}'
    //Example of DList array of strings:
    //-
 *
 *  'JSxxxx{"KList":
 *  [
 *  {
 *   "slot":<string>,   //hex
 *   "a64":<string>,    //address64, string
 *   "a16":<string>,    //address16, string
 *   "F":<string>,      //multFast, hex
 *   "S":<string>,      //multSlow, hex
 *   "M":<string>       //mode, hex
 *  },
 *  {
 *   "slot":<string>,   //hex
 *   "a64":<string>,    //address64, string
 *   "a16":<string>,    //address16, string
 *   "F":<string>,      //multFast, hex
 *   "S":<string>,      //multSlow, hex
 *   "M":<string>       //mode, hex
 *  }
 *  ]}'
    //Example of KList array of objects:
    //-
 *
 *
 *  'JSxxxx{"TWR":
 *    {     "a16":%04X, //addr16
 *          "R":%d,//range num
 *          "T":%d,//sys timestamp of Final WRTO Node's SuperFrame start, us
 *          "D":%f,//distance
 *          "P":%f,//raw pdoa
 *          "Xcm":%f,//X distance w.r.t. node cm,
 *          "Ycm":%f,//Y distance w.r.t. node cm,
 *          "O":%f,//clock offset in hundreds part of ppm
 *          "V":%d //service message data from the tag: (stationary, etc)
 *          "X":%d //service message data from the tag: (stationary, etc)
 *          "Y":%d //service message data from the tag: (stationary, etc)
 *          "Z":%d //service message data from the tag: (stationary, etc)
 *    }
 *   }'
   //Example of TWR:
   //JS  xx{"TWR": {"a16":"2E5C","R":3,"T":8605,"D":343,"P":1695,"Xcm":165,"Ycm":165,"O":14,"V":1,"X":53015,"Y":60972,"Z":10797}}
 *
 *  Send Service message from Node:
 *  Currently sending stationary to the PC
 *
 *  'JSxxxx{"SN":
 *    {     "a16": %04X, //addr16 of the Node
 *            "V":%d //service message from the Node (stationary is bit 0)
 *          "X":%d //Normalized accel data X from the Node, mg
 *          "Y":%d //Normalized accel data Y from the Node, mg
 *          "Z":%d //Normalized accel data Z from the Node, mg
 *    }
 *   }'
   //Example of SN:
   //JS0035{"SN": {"a16":"0001","V":0,"X":-458,"Y":-83,"Z":876}}
*/

    QString s = QString::fromLatin1(st);    //this is input from COM-port : JSON string
    QJsonDocument json = QJsonDocument::fromJson(s.toUtf8());
    QJsonObject   obj = json.object();

    QString newAddr64 = obj.value("NewTag").toString();

    QJsonArray DList  = obj.value("DList").toArray();
    QJsonArray KList  = obj.value("KList").toArray();
    QJsonObject SysCalib  = obj.value("Calibration").toObject();

    QJsonObject tagAdded  = obj.value("TagAdded").toObject();
    QString tagDeleted  = obj.value("TagDeleted").toString();
    QJsonObject TWR  = obj.value("TWR").toObject();
    QJsonObject SN  =  obj.value("SN").toObject();

//        qDebug() << "JS string:" << s;


    if(SysCalib.size()>0)
    {
        qDebug() << QString("Node response: Calibration\r\n") << st;

        calib_data_t  calib;
        fromObjToCalibData(&SysCalib, &calib);

        RTLSDisplayApplication::client()->updatePDOAandRangeOffset(calib.pdoaOffset, calib.distOffset);
    }

    /* extract data from TWR object */
    if(TWR.size()>0)
    {
        tag_data_t tag;
        fromTwrObjToTagData(&TWR, &tag);

        RTLSDisplayApplication::client()->processRangeAndPDOAReport(
                                               tag.addr16,
                                               tag.twr.rangeNum,
                                               tag.twr.dist_m,
                                               tag.twr.xdist_m,
                                               tag.twr.ydist_m,
                                               tag.twr.pdoa_deg,
                                               tag.twr.vData,
                                               tag.twr.accX,
                                               tag.twr.accY,
                                               tag.twr.accZ);

    }

    if(tagDeleted.size()>0)
    {
        qDebug() << "Node response: TagDeleted\r\n";
    }

    if(tagAdded.size()>0)
    {
        tag_data_t tag;
        fromObjToTagData(&tagAdded, &tag);

        RTLSDisplayApplication::client()->updateTagAddr16(tag.addr64, tag.addr16);

        qDebug() << QString("Node response: TagAdded\r\n");
    }

    if(newAddr64.size()>0)
    {
        bool ok;
        quint64 addr64  = newAddr64.toULongLong(&ok, 16);

        qDebug() << "11111111111111111111111" << newAddr64 << addr64;

        //add new tag into the tag list 64/16 bit address pair
        RTLSDisplayApplication::client()->addTagToList(addr64);

    }

    if(!DList.empty())
    {
        qDebug() << QString("Node response: DList\r\n");

        for (int i=0; i< DList.size(); i++)
        {
            bool ok;
            quint64 addr64  = DList[i].toString().toULongLong(&ok, 16);

            //add new tag into the tag list 64/16 bit address pair
            RTLSDisplayApplication::client()->addTagToList(addr64);
        }
    }

    if(!KList.empty())
    {
        qDebug() << QString("Node response: KList\r\n");

        for(int i=0; i< KList.size(); i++)
        {
            QJsonObject tobj = KList[i].toObject();
            tag_data_t  tag;

            fromObjToTagData(&tobj, &tag);
            RTLSDisplayApplication::client()->addTagFromKList(tag.slot,
                                                                 tag.addr64, tag.addr16,
                                                                 tag.multFast, tag.multSlow,
                                                                 tag.mode);
        }
    }

	return (0);
}

/* brief
 *      handles input from COM-port : JSON string
 *      and extracts from Info object deviced and version parameters
 *
 */
void check_json_version(const QByteArray st, QString *device, QString *version)
{
    QString s = QString::fromLatin1(st);
    QJsonDocument json = QJsonDocument::fromJson(s.toUtf8());
    QJsonObject   obj = json.object();
    QJsonObject   Info  = obj.value("Info").toObject();

    qDebug() << QString("Node response: Info:\r\n");

    QString tmp = Info.value("Version").toString();
    if(tmp.length())
    {
        *version = tmp;
    }
    tmp  = Info.value("Device").toString();
    if(tmp.length())
    {
        *device = tmp;
    }
}
