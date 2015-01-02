#ifndef SPINEMLSPLITTER_H
#define SPINEMLSPLITTER_H

#include <QtXml/QXmlStreamReader>
#include <QHash>
#include <QFile>
#include <QElapsedTimer>
#include "modelobjects.h"
#include "writer.h"
#include "infoparser.h"
#include "parser.h"

#define MAX_POPULATION_SIZE 100

#define PARSER_DEBUG_OUTPUT 0
#define SPLITTER_DEBUG_OUTPUT 0

//int divide with ceil without cast to double
#define UINT_DIV_CEIL(x,y) ((x + y - 1) / y)

class SpineMLSplitter
{
public:
    SpineMLSplitter(bool parallel = true, bool formatted_output = true, bool silent = false, WriterMode = WRITER_MODE_XML);
    ~SpineMLSplitter();

    void split(QString experiment_input_filename, QString network_output_filename);

    uint getSplitPopulationCount();
    uint getSplitProjectionCount();
    uint getSplitInputCount();


    Parser *getParser();

    uint getSplitTime();
    uint getTotalTime();


protected:

    void parseExperimentNetwork(Experiment* experiment, QString network_output_filename);
    void parseExperimentFile(QString experiment_input_filename, QString network_output_filename);
    //population full parsing
    void parseAndSplitPopulations();   //TODO: Refactor to parser!!!


    //splitter
    void splitPopulation(Population *population, uint component_size);
    void splitPopulationExplicit(Population *population, uint component_size);
    void splitNeuron(Neuron *neuron, Neuron *sub_neuron, uint sub_pop_index, uint sub_pop_count);
    void splitInputs(Component *componenent, Component *sub_componenent, uint sub_comp_index, uint sub_comp_size);
    void splitProjections(Population *population, Population *sub_pop, uint sub_pop_index);
    void splitWeightUpdate(Synapse *synapse, Synapse *sub_synapse, uint sub_pop_index, uint sub_pop_size, uint pop_size, uint target_sub_pop_index, uint target_sub_pop_size, uint target_pop_size);
    void splitPostsynapse(Synapse *synapse, Synapse *sub_synapse, uint sub_pop_index, uint sub_pop_size, uint target_sub_pop_index, uint target_sub_pop_size);
    void splitProperties(Component *component, Component *sub_component, uint sub_comp_index, uint sub_comp_size, uint target_sub_pop_index=0, uint target_sub_pop_size=0, uint target_pop_size=0); //dst_sub_pop_index & dst_sub_pop_size required only for synapse and postsynaspe, dst_pop_size required only for synapse
    //splitter helper functions
    PropertyValue *cloneDelayPropertyValue(PropertyValue *delay);
    Projection *getSubProjection(Population *sub_population, QString dst_sub_population_name);           //gets an existing projection if one exists otherwise creates a new one
    Input *getSubInput(Input *input, Component *sub_componenent, QString src_unique_name, QString src_sub_comp_name, uint &sub_input_count);             //gets an existing input if one exists otherwise creates a new one



private:
    QString getSubName(QString name, uint sub_index);


private:
    bool parallel;
    bool formatted_output;
    bool silent;
    WriterMode mode;


    uint split_populations;
    uint split_projections;
    uint split_inputs;

    QXmlStreamReader xml_src;
    SpineMLWriter *writer;
    InfoParser *info_parser;
    Parser *parser;

    qint64 split_time;
    QElapsedTimer timer;
    qint64 temp_time;
};

#endif // SPINEMLSPLITTER_H
