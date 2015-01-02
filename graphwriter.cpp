#include "graphwriter.h"

GraphWriter::GraphWriter(QString output_filename, InfoParser *info)
    : SpineMLWriter(output_filename)
{
    out.setDevice(output_file);
    this->info = info;
}

void GraphWriter::writeDocumentStart()
{
    out << "// Neato graph produced by SpineML splitter" << endl;
    out << "// Example neato usage: neato -Tpng graph.dot -o graph.png" << endl << endl;
    out << "graph G {" << endl;
    out << "\toverlap=scale" << endl;
    out << "\tsplines=true" << endl;
    if (info->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC)
        out << "\tdir=forward" << endl;
    else
        out << "\tedge [dir=back]" << endl;
}

void GraphWriter::writeDocuemntEnd()
{
    out << "}" << endl;
}

void GraphWriter::writePopulation(Population *sub_population, Population *)
{
    QSet <QString> connections;
    QString sub_pop_name_safe = sub_population->neuron->name;
    sub_pop_name_safe = sub_pop_name_safe.replace(" ", "_");

    //interrupts
    if ((sub_population->projections.values().size() > 0) || (sub_population->neuron->inputs.values().size() > 0))
    //synapse nodes
    for (int p=0; p<sub_population->projections.values().size(); p++){
        Projection * sub_proj = sub_population->projections.values()[p];
        QString sub_proj_pop_name_safe =  sub_proj->proj_population;
        sub_proj_pop_name_safe = sub_proj_pop_name_safe.replace(" ", "_");
        QString proj = "\t%1 -- %2";
        proj = proj.arg(sub_pop_name_safe).arg(sub_proj_pop_name_safe);
        connections.insert(proj);
    }
    //input nodes to neuron
    for (int i=0; i<sub_population->neuron->inputs.values().size(); i++){
        Input * sub_input = sub_population->neuron->inputs.values()[i];
        QString sub_inp_pop_name_safe = sub_input->src;
        sub_inp_pop_name_safe = sub_inp_pop_name_safe.replace(" ", "_");
        QString input = "\t%1 -- %2";
        input = input.arg(sub_pop_name_safe).arg(sub_inp_pop_name_safe);
        connections.insert(input);
    }
    //TODO INPUT NODES TO PSP
    //output unique connections
    for (int i=0; i<connections.values().size(); i++){
        QString conn = connections.values()[i];
        out << conn << endl;
    }
}
