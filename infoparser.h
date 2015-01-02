#ifndef INFOPARSER_H
#define INFOPARSER_H

#include <QXmlStreamReader>


#include "modelobjects.h"

typedef enum
{
    SPLITMODE_UNDEFINED,
    SPLITMODE_PROJ_DEF_AT_SRC,
    SPLITMODE_PROJ_DEF_AT_DST
}SplitterMode;

class InfoParser
{
public:
    InfoParser(QXmlStreamReader *xml_src);
    ~InfoParser();

public:
    void parse();
    ComponentInfo *getComponentInfo(QString name);
    PopulationInfo *getPopulationInfo(QString pop_name);
    PopulationInfo *getUnsplitPopulationInfo(QString sub_pop_name);
    uint getSubPopulationIndex(QString sub_pop_name);
    QList<QString> getActiveSourcePorts(QString population_name);

    bool componentExists(QString name);
    SplitterMode getSplitterMode();

    void addPopulationInfo(PopulationInfo *pop_info);

protected:
    //population info parsing
    void parsePopulationInfo();
    ComponentInfo* parseNeuronInfo();
    void parseProjectionInfo(ComponentInfo* population_info);
    void parseSynapseInfo(QString population_name, QString proj_population, uint src_pop_size, uint dst_pop_size);
    void parseInput(QString src_name, bool ps_input=false);
    ConnectivityType parseConnectivityInfo(uint *connection_instances_count);

private:
    SplitterMode splitter_mode;
    QXmlStreamReader *xml;
    QHash<QString,ComponentInfo*> component_info;
    QHash<QString, QString> port_inputs; //ports used by inputs and projections by source name (one to many)
    uint population_count;
    uint sub_population_count;
};

#endif // INFOPARSER_H
