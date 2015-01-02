#ifndef SPINEMLXMLWRITER_H
#define SPINEMLXMLWRITER_H

#include <QXmlStreamWriter>

#include "writer.h"

class SpineMLXMLWriter : public SpineMLWriter
{
public:
    SpineMLXMLWriter(QString output_filename, bool formatted_output);

    void writeDocumentStart();
    void writeDocuemntEnd();

    void writePopulation(Population *sub_population, Population *population = NULL);
    void writeNeuron(Neuron *neuron);
    void writeProperty(Property *property);
    void writeDelay(PropertyValue *delay);
    void writeInput(Input *input);
    void writeProjection(Projection *projection);
    void writeSynapse(Synapse *synapse);
    void writeConnection(AbstractionConnection *connectivity);
    void writeWeightUpdate(WeightUpdate *weight_update);
    void writePostsynapse(Postsynapse *postsynapse);

private:
    QXmlStreamWriter xml_dst;
    bool formatted_output;
};

#endif // SPINEMLXMLWRITER_H
