#include "xmlwriter.h"

#include <iostream>

SpineMLXMLWriter::SpineMLXMLWriter(QString output_filename, bool formatted_output)
    : SpineMLWriter(output_filename)
{
    xml_dst.setDevice(output_file);
    this->formatted_output = formatted_output;
    if (formatted_output)
        xml_dst.setAutoFormatting(true);
}

void SpineMLXMLWriter::writeDocumentStart()
{
    xml_dst.writeStartDocument();
    xml_dst.writeStartElement("LL:SpineML");
    xml_dst.writeAttribute("xmlns", "http://www.shef.ac.uk/SpineMLNetworkLayer");
    xml_dst.writeAttribute("xmlns:LL", "http://www.shef.ac.uk/SpineMLLowLevelNetworkLayer");
    xml_dst.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    xml_dst.writeAttribute("xsi:schemaLocation", "http://www.shef.ac.uk/SpineMLNetworkLayer SpineMLNetworkLayer.xsd SpineMLLowLevelNetworkLayer SpineMLLowLevelNetworkLayer.xsd");
}

void SpineMLXMLWriter::writeDocuemntEnd()
{
    xml_dst.writeEndElement();  //spineml
    xml_dst.writeEndDocument();
}


void SpineMLXMLWriter::writePopulation(Population *sub_population, Population *)
{
    xml_dst.writeStartElement("LL:Population");
    writeNeuron(sub_population->neuron);
    for (int i=0; i<sub_population->projections.values().size();i++)
        writeProjection(sub_population->projections.values().at(i));
    xml_dst.writeEndElement(); //population
}

void SpineMLXMLWriter::writeNeuron(Neuron *neuron)
{
    xml_dst.writeStartElement("LL:Neuron");
    xml_dst.writeAttribute("name", neuron->name);
    xml_dst.writeAttribute("url", neuron->definition_url);
    xml_dst.writeAttribute("size", QString::number(neuron->size));

    //properties
    for (int i=0; i<neuron->properties.size();i++)
        writeProperty(neuron->properties.at(i));

    //inputs
    for(int i=0; i<neuron->inputs.size();i++)
        writeInput(neuron->inputs.values().at(i));

    xml_dst.writeEndElement(); //neuron
}

void SpineMLXMLWriter::writeProperty(Property *property)
{
    xml_dst.writeStartElement("Property");
    xml_dst.writeAttribute("name", property->name);
    if (property->dimension != "")
        xml_dst.writeAttribute("dimension", property->dimension);

    switch(property->value->Type())
    {
        case(FIXED_VALUE_TYPE):{
            FixedPropertyValue *value = (FixedPropertyValue*)property->value;
            xml_dst.writeStartElement("FixedValue");
            xml_dst.writeAttribute("value", QString::number(value->value));
            xml_dst.writeEndElement(); //FixedValue
            break;
        }
        case(VALUE_LIST_TYPE):{
            PropertyValueList *value = (PropertyValueList*)property->value;
            xml_dst.writeStartElement("ValueList");
            for(int i=0; i<value->valueInstances.values().size();i++){
                PropertyValueInstance *inst = value->valueInstances.values().at(i);
                xml_dst.writeStartElement("Value");
                xml_dst.writeAttribute("index", QString::number(inst->index));
                xml_dst.writeAttribute("value", QString::number(inst->value));
                xml_dst.writeEndElement(); //Value
            }
            xml_dst.writeEndElement(); //valueList
            break;
        }
        case(UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE):{
            UniformDistPropertyValue *value = (UniformDistPropertyValue*)property->value;
            xml_dst.writeStartElement("UniformDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("minimum", QString::number(value->minimum));
            xml_dst.writeAttribute("maximum", QString::number(value->maximum));
            xml_dst.writeEndElement(); //UniformDistribution
            break;
        }
        case(NORMAL_DISTRIBUTION_STOCHASTIC_TYPE):{
            NormalDistPropertyValue *value = (NormalDistPropertyValue*)property->value;
            xml_dst.writeStartElement("NormalDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("mean", QString::number(value->mean));
            xml_dst.writeAttribute("variance", QString::number(value->variance));
            xml_dst.writeEndElement(); //NormalValue
            break;
        }
        case(POISSON_DISTRIBUTION_STOCHASTIC_TYPE):{
            PoissonDistPropertyValue *value = (PoissonDistPropertyValue*)property->value;
            xml_dst.writeStartElement("PoissonDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("mean", QString::number(value->mean));
            xml_dst.writeEndElement(); //PoissonDistribution
            break;
        }
        default:{
            std::cerr << "Error: Unknown Property value type found! Should have found out before now!" << std::endl;
            exit(0);
            break;
        }
    }

    xml_dst.writeEndElement(); //Property
}

void SpineMLXMLWriter::writeDelay(PropertyValue *delay)
{
    if (delay == NULL)
        return;

    xml_dst.writeStartElement("Delay");
    switch(delay->Type())
    {
        case(FIXED_VALUE_TYPE):{
            FixedPropertyValue *value = (FixedPropertyValue*)delay;
            xml_dst.writeStartElement("FixedValue");
            xml_dst.writeAttribute("value", QString::number(value->value));
            xml_dst.writeEndElement(); //FixedValue
            break;
        }
        case(UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE):{
            UniformDistPropertyValue *value = (UniformDistPropertyValue*)delay;
            xml_dst.writeStartElement("UniformDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("minimum", QString::number(value->minimum));
            xml_dst.writeAttribute("maximum", QString::number(value->maximum));
            xml_dst.writeEndElement(); //UniformDistribution
            break;
        }
        case(NORMAL_DISTRIBUTION_STOCHASTIC_TYPE):{
            NormalDistPropertyValue *value = (NormalDistPropertyValue*)delay;
            xml_dst.writeStartElement("NormalDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("mean", QString::number(value->mean));
            xml_dst.writeAttribute("variance", QString::number(value->variance));
            xml_dst.writeEndElement(); //NormalValue
            break;
        }
        case(POISSON_DISTRIBUTION_STOCHASTIC_TYPE):{
            PoissonDistPropertyValue *value = (PoissonDistPropertyValue*)delay;
            xml_dst.writeStartElement("PoissonDistribution");
            xml_dst.writeAttribute("seed", QString::number(value->seed));
            xml_dst.writeAttribute("mean", QString::number(value->mean));
            xml_dst.writeEndElement(); //PoissonDistribution
            break;
        }
        default:{
            //Cant get here. Only above types saved by splitter
            break;
        }
    }
    xml_dst.writeEndElement(); //delay
}

void SpineMLXMLWriter::writeInput(Input *input)
{
    xml_dst.writeStartElement("LL:Input");
    xml_dst.writeAttribute("src_port", input->src_port);
    xml_dst.writeAttribute("dst_port", input->dst_port);
    xml_dst.writeAttribute("src", input->src);
    if (input->remapping != NULL){
        writeConnection(input->remapping);
    }
    xml_dst.writeEndElement(); //input
}

void SpineMLXMLWriter::writeProjection(Projection *projection)
{
    xml_dst.writeStartElement("LL:Projection");
    xml_dst.writeAttribute("dst_population", projection->proj_population);
    //targets
    for (int i=0; i< projection->synapses.values().size(); i++)
        writeSynapse(projection->synapses.values().at(i));
    xml_dst.writeEndElement(); //Projection
}

void SpineMLXMLWriter::writeSynapse(Synapse *synapse)
{
    xml_dst.writeStartElement("LL:Synapse");
    writeConnection(synapse->connection);
    //synapse
    writeWeightUpdate(synapse->weightupdate);
    //postsynapse
    writePostsynapse(synapse->postsynapse);
    xml_dst.writeEndElement(); //Target
}

void SpineMLXMLWriter::writeConnection(AbstractionConnection *connectivity)
{
    if (connectivity == NULL)
        return;
    switch(connectivity->Type()){
        case(ALL_TO_ALL_CONNECTVITY_TYPE):{
            AllToAllConnection *all_to_all = (AllToAllConnection*) connectivity;
            xml_dst.writeStartElement("AllToAllConnection");
            writeDelay(all_to_all->delay);
            xml_dst.writeEndElement(); //AllToAllConnection
            break;
        }
        case(ONE_TO_ONE_CONNECTVITY_TYPE):{
            OneToOneConnection *one_to_one = (OneToOneConnection*) connectivity;
            xml_dst.writeStartElement("OneToOneConnection");
            writeDelay(one_to_one->delay);
            xml_dst.writeEndElement(); //OneToOneConnection
            break;
        }
        case(LIST_CONNECTVITY_TYPE):{
            ConnectionList *connection_list = (ConnectionList*) connectivity;
            xml_dst.writeStartElement("ConnectionList");
            for (int i=0 ; i<connection_list->connectionIndices.values().size(); i++){
                ConnectionInstance *inst = connection_list->connectionIndices.values().at(i);
                xml_dst.writeStartElement("Connection");
                xml_dst.writeAttribute("src_neuron", QString::number(inst->src_neuron));
                xml_dst.writeAttribute("dst_neuron", QString::number(inst->dst_neuron));
                xml_dst.writeAttribute("delay", QString::number(inst->delay));
                xml_dst.writeAttribute("index", QString::number(inst->index));
                xml_dst.writeEndElement(); //Connection
            }
            xml_dst.writeEndElement(); //ConnectionList
            break;
        }
        case(FIXED_PROBABILITY_CONNECTVITY_TYPE):{
            FixedProbabilityConnection *fixed = (FixedProbabilityConnection*)connectivity;
            xml_dst.writeStartElement("FixedProbabilityConnection");
            xml_dst.writeAttribute("probability", QString::number(fixed->probability));
            xml_dst.writeAttribute("seed", QString::number(fixed->seed));
            writeDelay(fixed->delay);
            xml_dst.writeEndElement(); //FixedProbabilityConnection
            break;
        }
        default:{
            std::cerr << "Error: Unsupported connectivity type found by writer!" << std::endl;
            exit(0);
            break;
        }
    }
}

void SpineMLXMLWriter::writeWeightUpdate(WeightUpdate *weight_update)
{
    xml_dst.writeStartElement("LL:WeightUpdate");
    xml_dst.writeAttribute("name", weight_update->name);
    xml_dst.writeAttribute("url", weight_update->definition_url);
    xml_dst.writeAttribute("input_src_port", weight_update->input_src_port);
    xml_dst.writeAttribute("input_dst_port", weight_update->input_dst_port);

    //properties
    for (int i=0; i<weight_update->properties.size();i++)
        writeProperty(weight_update->properties.at(i));

    //inputs
    for(int i=0; i<weight_update->inputs.size();i++)
        writeInput(weight_update->inputs.values().at(i));

    xml_dst.writeEndElement(); //Synapse
}

void SpineMLXMLWriter::writePostsynapse(Postsynapse *postsynapse)
{

    xml_dst.writeStartElement("LL:PostSynapse");

    xml_dst.writeAttribute("name", postsynapse->name);
    xml_dst.writeAttribute("url", postsynapse->definition_url);
    xml_dst.writeAttribute("input_src_port", postsynapse->input_src_port);
    xml_dst.writeAttribute("input_dst_port", postsynapse->input_dst_port);
    xml_dst.writeAttribute("output_src_port", postsynapse->output_src_port);
    xml_dst.writeAttribute("output_dst_port", postsynapse->output_dst_port);

    //properties
    for (int i=0; i<postsynapse->properties.size();i++)
        writeProperty(postsynapse->properties.at(i));

    //inputs
    for(int i=0; i<postsynapse->inputs.size();i++)
        writeInput(postsynapse->inputs.values().at(i));

    xml_dst.writeEndElement(); //PostSynapse
}


