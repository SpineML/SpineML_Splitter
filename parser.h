#ifndef PARSER_H
#define PARSER_H

#include <QXmlStreamReader>
#include "modelobjects.h"
#include "infoparser.h"

class Parser
{
public:
    Parser(QXmlStreamReader *xml_src, InfoParser *info_parser);

    Population* parsePopulation();
    Neuron* parseNeuron();
    Property* parseProperty(uint comp_size);
    Input* parseInput(Component* component, uint component_size);
    AbstractionConnection* parseConnectivity(uint max_src_index, uint max_dst_index, bool hash_instances_by_src);
    Projection* parseProjection(Neuron* neuron);
    Synapse* parseTarget(Neuron* neuron, uint dst_pop_size);
    WeightUpdate* parseSynapse(uint synapse_size);
    Postsynapse* parsePostsynapse(uint postsynapse_size);
    PropertyValue* parseDelayPropertyValue();
    Experiment* parseExperiment(QString input_path);
    LogOutput* parseLogOutput();

    //static functions (used by info parser and parser)

    static QString readStringElement(QXmlStreamReader *xml);
    static int readIntElement(QXmlStreamReader *xml);
    static double readDoubleElement(QXmlStreamReader *xml);
    static QString getStringAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional=false);
    static int getIntAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional=false);
    static double getDoubleAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional=false);
    static QString getSubName(QXmlStreamReader *xml, QString name, uint sub_index);

    //stats

    uint getParsedPopulationCount();
    uint getParsedProjectionCount();
    uint getParsedInputCount();

private:
    QXmlStreamReader *xml;
    InfoParser *info;

    uint parsed_populations;
    uint parsed_projections;
    uint parsed_inputs;
    uint parsed_instances;
};

#endif // PARSER_H
