
#include "infoparser.h"
#include "parser.h"
#include "splitter.h"
#include <QFile>
#include <iostream>
#include <QDebug>
#include <QStringList>


#define INFO_PARSER_DEBUG_OUTPUT 0

InfoParser::InfoParser(QXmlStreamReader *xml_src)
{
    splitter_mode = SPLITMODE_UNDEFINED;
    xml = xml_src;
    population_count = 1;
    sub_population_count = 1;
}

InfoParser::~InfoParser()
{
    for (int i=0; i<component_info.values().size(); i++)
    {
        delete component_info.values()[i];
    }
}


void InfoParser::parse()
{
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "*** Start Info Parsing";
    //xml input
    xml->readNextStartElement();
    if (!(xml->isStartElement() && xml->name() == "SpineML"))
    {
        std::cerr << "Error (line " << xml->lineNumber() << "): XML model file is not a SpineML LL document!" << std::endl;
        exit(0);
    }
    while (xml->readNextStartElement()) {
         if (xml->name() == "Population"){
             parsePopulationInfo();
         }
         else if (xml->name() == "ComponenentInstance"){
             std::cerr << "Error (line " << xml->lineNumber() << "): XML model file contains groups. These must be converted to populations!" << std::endl;
             exit(0);
         }
         else
             xml->skipCurrentElement();
    }
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "*** End Info Parsing";

    //update dimensions of weightupdate and postsynapses information
    for (int i=0; i<component_info.values().size(); i++){
        ComponentInfo *info  = component_info.values()[i];
        switch (info->Type()){
            case(COMPONENT_TYPE_WEIGHT_UPDATE):
            case(COMPONENT_TYPE_POSTSYNAPSE):{
                info->calculateDimensions(component_info);
                break;
            }
            default:{
                break;
            }
        }
        if (INFO_PARSER_DEBUG_OUTPUT)
            qDebug() << "Splitter: Pre-split component '" << info->name << "' size is " << info->size;
    }
}



void InfoParser::parsePopulationInfo()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Population");
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "Info Parser: Found Population on line " << xml->lineNumber();

    ComponentInfo* population_info;

    //neuron
    xml->readNextStartElement();
    if (xml->name() == "Neuron"){
        population_info = parseNeuronInfo();
    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'neuron' element in population instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }
    //projections
    while (xml->readNextStartElement()) {
        if (xml->name() == "Projection"){
            parseProjectionInfo(population_info);
        }
        else
            xml->skipCurrentElement();
    }

}

ComponentInfo *InfoParser::parseNeuronInfo()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Neuron");
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "Info Parser: Found neuron on line " << xml->lineNumber();

    PopulationInfo *pop_info = new PopulationInfo();

    pop_info->name = Parser::getStringAttribute(xml, "name");
    pop_info->size = Parser::getIntAttribute(xml, "size");
    pop_info->global_index = population_count++;
    pop_info->global_sub_start_index = sub_population_count;
    pop_info->splits = UINT_DIV_CEIL(pop_info->size, MAX_POPULATION_SIZE);
    sub_population_count += pop_info->splits;

    if (component_info.contains(pop_info->name)){
        std::cerr << "Error (line " << xml->lineNumber() << "): Duplicate component name '" <<  pop_info->name.toLocal8Bit().data() << "' found in model file." << std::endl;
        exit(0);
    }
    //add to hashmap
    component_info[pop_info->name] = (ComponentInfo*)pop_info;

    //parse any inputs
    while (xml->readNextStartElement()) {
        if (xml->name() == "Input"){
            parseInput(pop_info->name);
        }
        else
            xml->skipCurrentElement();
    }

    return pop_info;
}




void InfoParser::parseProjectionInfo(ComponentInfo *population_info)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Projection");
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "Info Parser: Found projection on line " << xml->lineNumber();

    QString proj_population = Parser::getStringAttribute(xml, "dst_population", true);

    //set mode and check for consistency
    if (proj_population == ""){
        proj_population = Parser::getStringAttribute(xml, "src_population");
        if (proj_population == ""){
            std::cerr << "Error (line " << xml->lineNumber() << "): No dst_population or src_population atrribute in 'Projection' element." << std::endl;
            exit(0);
        }else{
            //set src mode
            if (splitter_mode == SPLITMODE_PROJ_DEF_AT_SRC){
                std::cerr << "Error (line " << xml->lineNumber() << "): Projections using src_population and dst_population can not be mixed within the same network layer document." << std::endl;
                exit(0);
            }
            splitter_mode = SPLITMODE_PROJ_DEF_AT_DST;
        }
    }else{
        //set dst mode
        if (splitter_mode == SPLITMODE_PROJ_DEF_AT_DST){
            std::cerr << "Error (line " << xml->lineNumber() << "): Projections using src_population and dst_population can not be mixed within the same network layer document." << std::endl;
            exit(0);
        }
        splitter_mode = SPLITMODE_PROJ_DEF_AT_SRC;
    }

    while (xml->readNextStartElement()) {
        if (xml->name() == "Synapse"){
            if (splitter_mode == SPLITMODE_PROJ_DEF_AT_SRC)
                parseSynapseInfo(population_info->name, proj_population, population_info->size, 0); //0=unknown until after parse complete
            else //splitter_mode == SPLITMODE_PROJ_DEF_AT_DST
                parseSynapseInfo(population_info->name, proj_population, 0, population_info->size); //0=unknown until after parse complete
        }
        else
            xml->skipCurrentElement();
    }
}

void InfoParser::parseSynapseInfo(QString population_name, QString proj_population, uint src_pop_size, uint dst_pop_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Synapse");
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "Info Parser: Found target on line " << xml->lineNumber();

    //read connectivity
    uint explicit_connections = 0;
    ConnectivityType connection_type = parseConnectivityInfo(&explicit_connections);


    //synpase
    xml->readNextStartElement();
    if (xml->name() == "WeightUpdate"){
        //create synpase info
        WeightUpdateInfo *info = new WeightUpdateInfo();
        info->name = Parser::getStringAttribute(xml, "name");
        info->projPopulation = proj_population;
        info->size = 0;     // Size ignored as this will be calculated after full info parse!
        info->srcPopSize = src_pop_size;
        info->dstPopSize = dst_pop_size;
        info->connectivity = connection_type;
        info->connectionListCount = explicit_connections;
        //add to hashmap
        if (component_info.contains(info->name)){
            std::cerr << "Error (line " << xml->lineNumber() << "): Duplicate component name '" <<  info->name.toLocal8Bit().data() << "' found in model file." << std::endl;
            exit(0);
        }
        component_info[info->name] = (ComponentInfo*)info;
        //add to port hash
        if (getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC)
            port_inputs.insertMulti(population_name, Parser::getStringAttribute(xml, "input_src_port"));
        else
            port_inputs.insertMulti(proj_population, Parser::getStringAttribute(xml, "input_src_port"));

        //parse any inputs
        while (xml->readNextStartElement()) {
            if (xml->name() == "Input"){
                //error no inputs in weight updates supported
                std::cerr << "Error (line " << xml->lineNumber() << "): Inputs not supported in Weight Updated Component named '" <<  info->name.toLocal8Bit().data() << "' found in model file." << std::endl;
                exit(0);
            }
            else
                xml->skipCurrentElement();
        }

    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'Synapse' element in target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    //postsynapse
    xml->readNextStartElement();
    if (xml->name() == "PostSynapse"){
        //create postsynpase info
        PostsynapseInfo *info = new PostsynapseInfo();
        info->name = Parser::getStringAttribute(xml, "name");
        info->projPopulation = proj_population;
        info->size = dst_pop_size;// if 0 then will be calculated after full info parse!
        //add to hashmap
        if (component_info.contains(info->name)){
            std::cerr << "Error (line " << xml->lineNumber() << "): Duplicate component name '" <<  info->name.toLocal8Bit().data() << "' found in model file." << std::endl;
            exit(0);
        }
        component_info[info->name] = (ComponentInfo*)info;

        //parse any inputs
        while (xml->readNextStartElement()) {
            if (xml->name() == "Input"){
                parseInput(population_name, true);
            }
            else
                xml->skipCurrentElement();
        }

    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'PostSynapse' element in target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    xml->skipCurrentElement();

}

void InfoParser::parseInput(QString src_name, bool ps_input)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Input");
    if (INFO_PARSER_DEBUG_OUTPUT)
        qDebug() << "Info Parser: Found input on line " << xml->lineNumber();


    //atributes
    QString src = Parser::getStringAttribute(xml, "src");
    QString src_port = Parser::getStringAttribute(xml, "src_port");
    bool ignore = false;

    //ignore self inputs between PS and neuron where there is a one to one corellation (DAMSON specific)
    if ((ps_input)&&(src_name == src)){
        xml->readNextStartElement();    //connectivity
        if (xml->name() == "OneToOneConnection")
            ignore = true;
        xml->skipCurrentElement();      //skip out of connectivity
    }

    if (!ignore)
        port_inputs.insertMulti(src, src_port);

    xml->skipCurrentElement(); //skip out of input
}

ConnectivityType InfoParser::parseConnectivityInfo(uint *connection_instances_count)
{
    ConnectivityType type = NULL_CONNECTIVITY_TYPE;
    *connection_instances_count = 0;
    xml->readNextStartElement();
    if (xml->name() == "ConnectionList"){
        while (xml->readNextStartElement()) {
            if (xml->name() == "BinaryFile"){
                (*connection_instances_count) = Parser::getIntAttribute(xml, "num_connections");
                xml->skipCurrentElement();
                break;
            }
            else if (xml->name() == "Connection"){
                (*connection_instances_count)++;
            }
            else
                xml->skipCurrentElement();

        }
        type = LIST_CONNECTVITY_TYPE;
    }else if (xml->name() == "OneToOneConnection") {
        //xml->skipCurrentElement();
        type = ONE_TO_ONE_CONNECTVITY_TYPE;
    }else if (xml->name() == "AllToAllConnection") {
        //xml->skipCurrentElement();
        type = ALL_TO_ALL_CONNECTVITY_TYPE;
    }else if (xml->name() == "FixedProbabilityConnection") {
        //xml->skipCurrentElement();
        type = FIXED_PROBABILITY_CONNECTVITY_TYPE;
    }else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected a Connection type element in target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    xml->skipCurrentElement();
    return type;
}

SplitterMode InfoParser::getSplitterMode()
{
    return splitter_mode;
}

void InfoParser::addPopulationInfo(PopulationInfo *pop_info)
{
    if (component_info.contains(pop_info->name)){
        std::cerr << "Error: Duplicate component name '" <<  pop_info->name.toLocal8Bit().data() << "' found added to component info." << std::endl;
        exit(0);
    }
    component_info[pop_info->name] = (ComponentInfo*)pop_info;
}

ComponentInfo *InfoParser::getComponentInfo(QString name)
{
    return component_info[name];
}

PopulationInfo *InfoParser::getPopulationInfo(QString pop_name)
{
    ComponentInfo* info = component_info[pop_name];
    if(info->Type() == COMPONENT_TYPE_POPULATION)
    {
        return (PopulationInfo*)info;
    }
    else
    {
        std::cerr << "Error: Component is not a population '" <<  pop_name.toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }
}

PopulationInfo *InfoParser::getUnsplitPopulationInfo(QString sub_pop_name)
{
    QStringList sub_pop = sub_pop_name.split("_");
    if (sub_pop.size() == 2){
        QString pop_name = sub_pop.at(0);
        if (!component_info.contains(pop_name)){
            std::cerr << "Error: Unable to find unsplit population '" <<  pop_name.toLocal8Bit().data() << "' from sub population '" <<  sub_pop_name.toLocal8Bit().data() << "'" << std::endl;
            exit(0);
        }
        ComponentInfo* info = component_info[pop_name];
        if(info->Type() == COMPONENT_TYPE_POPULATION)
        {
            return (PopulationInfo*)info;
        }
        else
        {
            std::cerr << "Error: Component is not a population '" <<  sub_pop_name.toLocal8Bit().data() << "'" << std::endl;
            exit(0);
        }
    }else{
        std::cerr << "Error: Malformed sub population name '" <<  sub_pop_name.toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }
}

uint InfoParser::getSubPopulationIndex(QString sub_pop_name)
{
    QStringList sub_pop = sub_pop_name.split("_");
    if (sub_pop.size() == 2){
        QString sub = sub_pop.at(1);
        sub = sub.remove(0,3);
        return sub.toInt();
    }else{
        std::cerr << "Error: Malformed sub population name '" <<  sub_pop_name.toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

}

QList<QString> InfoParser::getActiveSourcePorts(QString population_name)
{
    return port_inputs.values(population_name);
}

bool InfoParser::componentExists(QString name)
{
    return component_info.contains(name);
}
