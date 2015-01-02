#include "modelobjects.h"
#include <QDebug>
#include <iostream>
#include <QStringList>

Component::~Component(){
    for (int i=0;i<properties.size();i++)
        delete properties[i];
    for (int i=0;i<inputs.values().size();i++)
        delete inputs.values()[i];
}

Population::Population()
{
    neuron = NULL;
}

Population::~Population(){
    for (int i=0;i<projections.values().size();i++)
        delete projections.values()[i];
    if (neuron != NULL)
        delete neuron;
}

PropertyValueList::~PropertyValueList(){
    for (int i=0;i<valueInstances.size();i++)
        delete valueInstances[i];
}

Projection::~Projection(){
    for (int i=0;i<synapses.values().size();i++)
        delete synapses.values()[i];
}

ConnectionList::~ConnectionList(){
    /*for (int i=0;i<connectionInstances.size();i++)
        delete connectionInstances[i];
        */
    for (int i=0;i<connectionMatrix.keys().size();i++){
        uint x = connectionMatrix.keys()[i];
        for (int j=0;j<connectionMatrix[x].keys().size();j++){
            uint y = connectionMatrix[x].keys()[j];
            delete connectionMatrix[x][y];
        }
    }
}

Input::Input()
{
    remapping = NULL;
    sub_inp_max = 0;
}

Input::~Input()
{
    if (remapping != NULL)
        delete remapping;
}

Synapse::Synapse()
{
    connection = NULL;
    weightupdate = NULL;
    postsynapse = NULL;
    _sub_syn_max = 0;
}

Synapse::~Synapse()
{
    if (connection != NULL)
        delete connection;
    if (weightupdate != NULL)
        delete weightupdate;
    if (postsynapse != NULL)
        delete postsynapse;
}

Property::Property()
{
    value = NULL;
}

Property::~Property()
{
    if (value != NULL)
        delete value;
}



void PopulationInfo::calculateDimensions(QHash<QString, ComponentInfo *> &)
{
    //not needed
}


void WeightUpdateInfo::calculateDimensions(QHash<QString, ComponentInfo *> &component_info)
{

    ComponentInfo *proj_pop = NULL;
    if (component_info.contains(projPopulation))
        proj_pop = component_info[projPopulation];
    else{
        std::cerr << "Error: Error calculating synapse size. Source or Destination population '" << projPopulation.toLocal8Bit().data() << "' does exist" << std::endl;
        exit(0);
    }
    if (proj_pop->Type() != COMPONENT_TYPE_POPULATION){
        std::cerr << "Error: Error calculating synapse size. Source or Destination population '" << projPopulation.toLocal8Bit().data() << "' must be a neuron name" << std::endl;
        exit(0);
    }

    //update sizes depending on src or dst based projections
    if (srcPopSize == 0){   //this means it is a projection specified at dst
        srcPopSize = proj_pop->size;
    }else{
        dstPopSize = proj_pop->size;
    }
    size = dstPopSize;

    switch(connectivity)
    {
        case(ALL_TO_ALL_CONNECTVITY_TYPE):
        case(FIXED_PROBABILITY_CONNECTVITY_TYPE):{
            size *= srcPopSize;
            break;
        }
        case(ONE_TO_ONE_CONNECTVITY_TYPE):{
            if (size != srcPopSize){
                std::cerr << "Error: Error calculating synapse size. Destination population '" << projPopulation.toLocal8Bit().data() << "' size does not match source size of '" << srcPopSize << "'!" << std::endl;
                exit(0);
            }
            break;
        }
        default:{
            //List types must have explictity set the size in advance
            break;
        }
    }

}

void PostsynapseInfo::calculateDimensions(QHash<QString, ComponentInfo *> &component_info)
{
    ComponentInfo *proj_pop = NULL;
    if (component_info.contains(projPopulation))
        proj_pop = component_info[projPopulation];
    else{
        std::cerr << "Error: Error calculating synapse size. Source or Destination population '" << projPopulation.toLocal8Bit().data() << "' does not exist" << std::endl;
        exit(0);
    }
    if (proj_pop->Type() != COMPONENT_TYPE_POPULATION){
        std::cerr << "Error: Error calculating synapse size. Source or Destination population '" << projPopulation.toLocal8Bit().data() << "' must be a neuron name" << std::endl;
        exit(0);
    }

    if (size == 0){ //projection defined at src (otherwise already completed)
        size = proj_pop->size;
    }
}

AbstractionConnection::AbstractionConnection()
{
    delay = NULL;
}

AbstractionConnection::~AbstractionConnection()
{
    if (delay != NULL)
        delete delay;
}

QString WeightUpdate::unsplitName(QString split_name)
{
    return split_name.split("_sub").at(0);
}

Experiment::Experiment()
{
    duration = 0;
    time_step = 0;
}

Experiment::~Experiment()
{
    for (int i=0; i< outputs.values().size(); i++){
        delete outputs.values()[i];
    }
}

LogOutput::LogOutput()
{
    name = "";
    target = "";
    port = "";
    start_time = 0;
    end_time = 0;
}

LogOutput::~LogOutput()
{
}








