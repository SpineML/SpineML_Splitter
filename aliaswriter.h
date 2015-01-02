#ifndef ALIASWRITER_H
#define ALIASWRITER_H

#include <QTextStream>
#include "writer.h"
#include "infoparser.h"

typedef enum{
    ALIAS_MODE_CONNECTION_DATA,
    ALIAS_MODE_DELAY_DATA
} ConnectionWriteMode;

class DamsonAliasWriter : public SpineMLWriter
{
public:
    DamsonAliasWriter(QString output_filename, InfoParser *info, Experiment* experiment);

    void writeDocumentStart();
    void writeDocuemntEnd();

    void writePopulation(Population *sub_population, Population *population = NULL);

private:
    void writeHashTableData(Population *sub_population, Population *population);
    void writeActivePortsData(QString unsplit_neuron_name);

    void writeAllExplicitNeuronPropertyData(Population* sub_population, QString unsplit_neuron_name);
    void writeAllExplicitPSPropertyData(Population* sub_population, QString unsplit_neuron_name);
    void writeAllExplicitWUPropertyData(Population* sub_population, Population* population);

    void writeAllExplicitSynapseData(Population* sub_population, Population* population);
    void writeSingleExplicitSynapseData(ConnectionWriteMode mode, Projection* projection, uint proj_target_sub_count, Synapse *synapse, Population* sub_population, uint sub_pop_index);

    void writeAllExplicitNeuronInputData(Population *sub_population, Population* population);
    void writeSingleExplicitNeuronInputData(ConnectionWriteMode mode, QString unsplit_component_name, Input *unsplit_input, QHash<QString, Input *> &split_inputs);

    void writeAllExplicitPSInputData(Population *sub_population, Population* population);
    void writeSingleExplicitPSInputData(ConnectionWriteMode mode, QString unsplit_component_name, Input *unsplit_input, Projection *projection, uint proj_target_sub_count, Synapse *synapse, Population *sub_population, uint sub_pop_index);

    void writeInterruptData(Population* sub_population);
    void writeLogOutputData(Population* sub_population, Population* population, Experiment* experiment);

    void errorCheckSynapse(Synapse *synapse, Synapse *sub_synapse, QString sub_syn_name);
    void outputEmptyMatrix(uint dim, uint tab_depth);
    QString openSubArray(uint depth);
    QString closeSubArray(uint depth, uint i, uint c);
    QString sanitizeName(QString name, QString replacement = "_");
    template <class T> QString arrayValue(T item, uint i, uint c);

private:
    QTextStream out;
    InfoParser *info;
    Experiment *experiment;

};


class DAMSONInputHash{
public:
    uint src_node_number;
    uint src_switch_value;
    uint src_split_index;
public:
    bool operator< (const DAMSONInputHash &h2) const;
    bool operator== (const DAMSONInputHash &h2) const;
    QString toString() const;
} ;

#endif // ALIASWRITER_H
