#include "parser.h"

#include <QFile>
#include <iostream>
#include <QDebug>
#include <QStringList>

#define PARSER_DEBUG_OUTPUT 0
#define XML_READING_DEBUG_OUTPUT 0

Parser::Parser(QXmlStreamReader *xml_src, InfoParser *info_parser)
{
    parsed_populations = 0;
    parsed_projections = 0;
    parsed_inputs      = 0;
    parsed_instances   = 0;

    xml = xml_src;
    info = info_parser;
}


Population *Parser::parsePopulation()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Population");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Population on line " << xml->lineNumber();

    Population *population = new Population();

    //neuron
    xml->readNextStartElement();
    if (xml->name() == "Neuron"){
        population->neuron = parseNeuron();
    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'neuron' element in population instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }
    //Projections
    uint proj_index = 0;
    while (xml->readNextStartElement()) {
        if (xml->name() == "Projection"){
            Projection *projection = parseProjection(population->neuron);
            projection->index = proj_index++;
            parsed_projections++;
            if (population->projections[projection->proj_population] != NULL){
                std::cerr << "Error (line " << xml->lineNumber() << "): Duplicate projection destination found in population" << std::endl;
                exit(0);
            }
            population->projections[projection->proj_population] = projection;
        }
        else
            xml->skipCurrentElement();
    }
    parsed_populations++;
    return population;
}

Neuron *Parser::parseNeuron()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Neuron");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Neuron on line " << xml->lineNumber();

    ComponentInfo * neuron_info= NULL;
    Neuron *neuron = new Neuron();

    //attributes
    neuron->name = Parser::getStringAttribute(xml, "name");
    neuron->size = Parser::getIntAttribute(xml, "size");
    neuron->definition_url = Parser::getStringAttribute(xml, "url");

    //get neuron info
    neuron_info = info->getComponentInfo(neuron->name);


    //parse inputs and properties
    while (xml->readNextStartElement()) {
        if (xml->name() == "Property"){
            neuron->properties.append(parseProperty(neuron_info->size));
        }else if (xml->name() == "Input"){
            Input *input = parseInput(neuron, neuron_info->size);
            QString src = "%1_%2_%3";
            src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
            neuron->inputs[src] = input;
        }
        else
            xml->skipCurrentElement();
    }

    return neuron;
}


Property* Parser::parseProperty(uint comp_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Property");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Property on line " << xml->lineNumber();

    Property* property = new Property();

    //name
    property->name = Parser::getStringAttribute(xml, "name");
    property->dimension = Parser::getStringAttribute(xml, "dimension", true);

    //value
    xml->readNextStartElement();
    if (xml->name() == "FixedValue"){
        FixedPropertyValue *prop_fixed = new FixedPropertyValue();
        prop_fixed->value = Parser::getDoubleAttribute(xml, "value");
        property->value = (PropertyValue*)prop_fixed;
        xml->skipCurrentElement();
    } else if (xml->name() == "ValueList"){
        PropertyValueList *prop_list = new PropertyValueList();
        //read value instances
        while (xml->readNextStartElement()) {
            if (xml->name() == "Value"){
                PropertyValueInstance *prop_inst = new PropertyValueInstance;
                //index
                prop_inst->index = Parser::getIntAttribute(xml, "index");
                if (comp_size <= prop_inst->index)  //set the max index
                {
                    std::cerr << "Warning (line " << xml->lineNumber() << "): Property index '" << prop_inst->index << "' exceeds maximum index value of '" << (comp_size-1) << "'. Property Instance will be ignored!" << std::endl;
                    delete prop_inst;
                    xml->skipCurrentElement();
                    continue;
                }
                //value
                prop_inst->value = Parser::getDoubleAttribute(xml, "value");
                //add to property list
                if (prop_list->valueInstances.contains(prop_inst->index))
                {
                    PropertyValueInstance *existing = prop_list->valueInstances[prop_inst->index];
                    if (existing->value == prop_inst->value)
                    {
                        std::cerr << "Warning(line " << xml->lineNumber() << "): Property instance duplicate detected with index " << prop_inst->index << " and same value of " << prop_inst->value << "." << std::endl;
                    }else{
                        std::cerr << "Warning(line " << xml->lineNumber() << "): Property instance duplicate detected with index " << prop_inst->index << ". Differing value " << prop_inst->value << " will be ignored!" << std::endl;
                    }
                }else{
                    prop_list->valueInstances[prop_inst->index] = (prop_inst);
                }
                xml->skipCurrentElement();
            }
            else
                xml->skipCurrentElement();

        }
        property->value = (PropertyValue*)prop_list;
    }  else  if (xml->name() == "UniformDistribution"){
        UniformDistPropertyValue *uniform_dist_value = new UniformDistPropertyValue();
        uniform_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        uniform_dist_value->minimum = Parser::getDoubleAttribute(xml, "minimum");
        uniform_dist_value->maximum = Parser::getDoubleAttribute(xml, "maximum");
        property->value = (PropertyValue*)uniform_dist_value;
        xml->skipCurrentElement();
    }else if(xml->name() == "NormalDistribution"){
        NormalDistPropertyValue *normal_dist_value = new NormalDistPropertyValue();
        normal_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        normal_dist_value->mean = Parser::getDoubleAttribute(xml, "mean");
        normal_dist_value->variance = Parser::getDoubleAttribute(xml, "variance");
        property->value = (PropertyValue*)normal_dist_value;
        xml->skipCurrentElement();
    }else if(xml->name() == "PoissonDistribution"){
        PoissonDistPropertyValue *poisson_dist_value = new PoissonDistPropertyValue();
        poisson_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        poisson_dist_value->mean = Parser::getDoubleAttribute(xml, "mean");
        property->value = (PropertyValue*)poisson_dist_value;
        xml->skipCurrentElement();
    }else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected a value type element in properties instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    xml->skipCurrentElement();
    return property;
}

Input *Parser::parseInput(Component *component, uint component_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Input");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found input on line " << xml->lineNumber();

    Input* input = new Input();
    ComponentInfo *src_info = NULL;

    //atributes
    input->src_port = Parser::getStringAttribute(xml, "src_port");
    input->dst_port = Parser::getStringAttribute(xml, "dst_port");
    input->src = Parser::getStringAttribute(xml, "src");
    //check input exists
    if (!info->componentExists(input->src)){
        std::cerr << "Error (line " << xml->lineNumber() << "): Input 'src' value of '" << input->src.toLocal8Bit().data() << "' is not found within the model " << std::endl;
        exit(0);
    }
    src_info = info->getComponentInfo(input->src);
    //check type of input (should already be done via xml validation!)
    if(src_info->Type() != COMPONENT_TYPE_POPULATION){
        std::cerr << "Error (line " << xml->lineNumber() << "): Input 'src' value of '" << input->src.toLocal8Bit().data() << "' must be a population (i.e. neuron)!" << std::endl;
        exit(0);
    }

    //remapping
    AbstractionConnection *conn = parseConnectivity(src_info->size-1, component_size-1, false); //hash by destination
    if (conn == NULL){
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected connection type element in remapping" << std::endl;
        exit(0);
    }
    //check dimensionality of remapping for one to one
    if (conn->Type() == ONE_TO_ONE_CONNECTVITY_TYPE){
        //check input size for dimensionality
        if (src_info->size != component_size){
            std::cerr <<  "Error (line " << xml->lineNumber() << "): Input size mismatch in between component size '" << component_size << "' and input '" << input->src.toLocal8Bit().data() << "' size '" << src_info->size << "'" << std::endl;
            exit(0);
        }
        if((component->Type() == COMPONENT_TYPE_WEIGHT_UPDATE)){
            std::cerr <<  "Error (line " << xml->lineNumber() << "): Componenents of type Synapse or Postsynapse cannot use a one to one remapping. Explicit lists should be used instead!" << std::endl;
            exit(0);
        }
    }

    input->remapping = conn;
    input->unsplit_index = component->inputs.values().size();
    xml->skipCurrentElement();

    parsed_inputs++;
    return input;
}

AbstractionConnection *Parser::parseConnectivity(uint max_src_index, uint max_dst_index, bool hash_instances_by_src)
{
    AbstractionConnection *connection = NULL;
    xml->readNextStartElement();
    if (xml->name() == "ConnectionList"){
        ConnectionList *conn_list = new ConnectionList();
        conn_list->delay = NULL;
        //read connection instances
        uint index_count = 0;
        while (xml->readNextStartElement()) {
            if (xml->name() == "BinaryFile"){
                int num_connections = Parser::getIntAttribute(xml, "num_connections");
                int delay_flag = Parser::getIntAttribute(xml, "explicit_delay_flag");
                QString filename = Parser::getStringAttribute(xml, "file_name");
                //open binary file
                QFile mfile(filename);
                if (!mfile.open(QFile::ReadOnly)) {
                    std::cerr << "Error (line " << xml->lineNumber() << "): Could not open binary connection file '" << filename.toLocal8Bit().data() << "'!" << std::endl;
                    exit(0);
                }
                QDataStream in(&mfile);
                in.setVersion(QDataStream::Qt_4_8);
                for (int i=0; i<num_connections; i++){
                    uint src_neuron;
                    uint dst_neuron;
                    uint delay;
                    ConnectionInstance *conn_inst;

                    conn_inst = new ConnectionInstance();
                    conn_inst->index = index_count++;

                    if (in.atEnd()){
                        std::cerr << "Error (line " << xml->lineNumber() << "): Unexpected end of open binary connection file '" << filename.toLocal8Bit().data() << "'!" << std::endl;
                        exit(0);
                    }

                    in >> src_neuron;
                    if (src_neuron > max_src_index){
                        std::cerr << "Error (binary file '" << filename.toLocal8Bit().data() << ")': src_neuron value '" << src_neuron << "' exceeds maximim value of '" << max_src_index << "'" << std::endl;
                        exit(0);
                    }
                    conn_inst->src_neuron = src_neuron;

                    in >> dst_neuron;
                    if (dst_neuron > max_dst_index){
                        std::cerr << "Error (line '" << filename.toLocal8Bit().data() << "''): dst_neuron value '" << dst_neuron << "' exceeds maximim value of '" << max_dst_index << "'" << std::endl;
                        exit(0);
                    }
                    conn_inst->dst_neuron = dst_neuron;

                    if (delay_flag){
                        in >> delay;
                        conn_inst->delay = delay;
                    }

                    //add to connection list
                    if (hash_instances_by_src){
                        conn_list->connectionIndices[conn_inst->index] = conn_inst;
                        if (conn_list->connectionMatrix[conn_inst->src_neuron][conn_inst->dst_neuron] != NULL){
                            std::cerr << "Error (binary file " << filename.toLocal8Bit().data() << "): duplicate connection found!"<< std::endl;
                            exit(0);
                        }
                        conn_list->connectionMatrix[conn_inst->src_neuron][conn_inst->dst_neuron] = conn_inst;
                    }
                    else{
                        conn_list->connectionIndices[conn_inst->index] = conn_inst;
                        if (conn_list->connectionMatrix[conn_inst->dst_neuron][conn_inst->src_neuron] != NULL){
                            std::cerr << "Error (binary file " << filename.toLocal8Bit().data() << "): duplicate connection found!"<< std::endl;
                            exit(0);
                        }
                        conn_list->connectionMatrix[conn_inst->dst_neuron][conn_inst->src_neuron] = conn_inst;
                    }
                    xml->skipCurrentElement();

                }
                mfile.close();
            }else if(xml->name() == "Delay"){
                if (conn_list->delay != NULL){
                    std::cerr << "Error (line " << xml->lineNumber() << "): Multiple Delay elements found for ConnectionList!" << std::endl;
                    exit(0);
                }
                conn_list->delay = parseDelayPropertyValue();
            }else if (xml->name() == "Connection"){
                ConnectionInstance *conn_inst = new ConnectionInstance;
                conn_inst->index = index_count++;

                //srcNeuron
                uint src_neuron = (uint)Parser::getIntAttribute(xml, "src_neuron");
                if (src_neuron > max_src_index){
                    std::cerr << "Error (line " << xml->lineNumber() << "): src_neuron value '" << src_neuron << "' exceeds maximim value of '" << max_src_index << "'" << std::endl;
                    exit(0);
                }
                conn_inst->src_neuron = src_neuron;

                //dstNeuron
                uint dst_neuron = (uint)Parser::getIntAttribute(xml, "dst_neuron");
                if (dst_neuron > max_dst_index){
                    std::cerr << "Error (line " << xml->lineNumber() << "): dst_neuron value '" << dst_neuron << "' exceeds maximim value of '" << max_dst_index << "'" << std::endl;
                    exit(0);
                }
                conn_inst->dst_neuron = dst_neuron;

                //delay
                conn_inst->delay = Parser::getDoubleAttribute(xml, "delay", true);

                //add to connection list
                if (hash_instances_by_src){
                    conn_list->connectionIndices[conn_inst->index] = conn_inst;
                    if (conn_list->connectionMatrix[conn_inst->src_neuron][conn_inst->dst_neuron] != NULL){
                        std::cerr << "Error (line " << xml->lineNumber() << "): duplicate connection found!"<< std::endl;
                        exit(0);
                    }
                    conn_list->connectionMatrix[conn_inst->src_neuron][conn_inst->dst_neuron] = conn_inst;
                }
                else{
                    conn_list->connectionIndices[conn_inst->index] = conn_inst;
                    if (conn_list->connectionMatrix[conn_inst->dst_neuron][conn_inst->src_neuron] != NULL){
                        std::cerr << "Error (line " << xml->lineNumber() << "): duplicate connection found!"<< std::endl;
                        exit(0);
                    }
                    conn_list->connectionMatrix[conn_inst->dst_neuron][conn_inst->src_neuron] = conn_inst;
                }
                xml->skipCurrentElement();
            }
            else
                xml->skipCurrentElement();

        }
        if (PARSER_DEBUG_OUTPUT)
            qDebug() << "Parser: Found ConnectionList";
        connection = (AbstractionConnection*) conn_list;
    }else if (xml->name() == "OneToOneConnection") {
        OneToOneConnection *one_to_one = new OneToOneConnection();
        //delay
        xml->readNextStartElement();
        if (xml->name() == "Delay"){
            one_to_one->delay = parseDelayPropertyValue();
        }else
        {
            std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'Delay' element in OneToOneConnection instead of '"<<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
            exit(0);
        }
        if (PARSER_DEBUG_OUTPUT)
            qDebug() << "Parser: Found OneToOneConnection ";
        xml->skipCurrentElement();
        connection = (AbstractionConnection*) one_to_one;
    }else if (xml->name() == "AllToAllConnection") {
        AllToAllConnection *all_to_all = new AllToAllConnection();
        //delay
        xml->readNextStartElement();
        if (xml->name() == "Delay"){
            all_to_all->delay = parseDelayPropertyValue();
        }else
        {
            std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'Delay' element in AllToAllConnection instead of '"<<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
            exit(0);
        }
        if (PARSER_DEBUG_OUTPUT)
            qDebug() << "Parser: Found AllToAllConnection";
        xml->skipCurrentElement();
        connection = (AbstractionConnection*) all_to_all;
    }else if (xml->name() == "FixedProbabilityConnection") {
        FixedProbabilityConnection *fixed_prob_conn = new FixedProbabilityConnection();

        //attributes
        fixed_prob_conn->probability = Parser::getDoubleAttribute(xml, "probability");
        fixed_prob_conn->seed = Parser::getIntAttribute(xml, "seed", true);

        //delay
        xml->readNextStartElement();
        if (xml->name() == "Delay"){
            fixed_prob_conn->delay = parseDelayPropertyValue();
        }else
        {
            std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'Delay' element in FixedProbabilityConnection instead of '"<<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
            exit(0);
        }

        if (PARSER_DEBUG_OUTPUT)
            qDebug() << "Parser: Found FixedProbabilityConnection";
        xml->skipCurrentElement();
        connection = (AbstractionConnection*) fixed_prob_conn;
    }else{
        std::cerr << "Error (line " << xml->lineNumber() << "): Unknown connectivity type '"<<  xml->name().toString().toLocal8Bit().data() << "' found!" << std::endl;
        exit(0);
    }

    //xml->skipCurrentElement(); //?????
    return connection;
}


Projection *Parser::parseProjection(Neuron* neuron)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Projection");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Projection on line " << xml->lineNumber();

    Projection *projection = new Projection();

    if (info->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC)
        projection->proj_population = Parser::getStringAttribute(xml, "dst_population");
    else if (info->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_DST)
        projection->proj_population = Parser::getStringAttribute(xml, "src_population");


    //check dst
    if (!info->componentExists(projection->proj_population)){
        std::cerr << "Error (line " << xml->lineNumber() << "): Projection destination '" << projection->proj_population.toLocal8Bit().data() << "' not found in model." << std::endl;
        exit(0);
    }
    ComponentInfo *dst_info = info->getComponentInfo(projection->proj_population);
    if (dst_info->Type() != COMPONENT_TYPE_POPULATION)
    {
        std::cerr << "Error (line " << xml->lineNumber() << "): Projection destination '" << projection->proj_population.toLocal8Bit().data() << "' is not a popultion name." << std::endl;
        exit(0);
    }

    //get info
    PopulationInfo * dst_pop_info = (PopulationInfo*)dst_info;

    while (xml->readNextStartElement()) {
        if (xml->name() == "Synapse"){
            Synapse *target = parseTarget(neuron, dst_pop_info->size);
            if (projection->synapses.contains(target->weightupdate->name))
            {
                std::cerr << "Error (line " << xml->lineNumber() << "): Duplicate Target Synapse name found in Projection" << std::endl;
                exit(0);
            }
            projection->synapses[target->weightupdate->name] = target;
        }
        else
            xml->skipCurrentElement();
    }

    return projection;
}

Synapse *Parser::parseTarget(Neuron *neuron, uint dst_pop_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Synapse");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Target on line " << xml->lineNumber();

    Synapse *target = new Synapse();

    //read connectivity
    AbstractionConnection *conn = parseConnectivity(neuron->size-1, dst_pop_size-1, true);
    if (conn == NULL)
    {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected Connectivity type element in Target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }
    target->connection = conn;

    //Synpase
    xml->readNextStartElement();
    if (xml->name() == "WeightUpdate"){
        uint synpase_size = dst_pop_size;
        switch(target->connection->Type()){
            case(ALL_TO_ALL_CONNECTVITY_TYPE):
            case(FIXED_PROBABILITY_CONNECTVITY_TYPE):{
                synpase_size *= neuron->size;
                break;
            }
            case(LIST_CONNECTVITY_TYPE):{
                ConnectionList * conn_list = (ConnectionList*) target->connection;
                synpase_size = conn_list->connectionIndices.values().size();
                break;
            }
            default: //ONE_TO_ONE proj_dst_size and neuron->size should be equal!
                break;
        }

        WeightUpdate *synapse = parseSynapse(synpase_size);
        synapse->target_connectivity = target->connection;
        target->weightupdate = synapse;
    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'Synapse' element in Target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    //Postsynapse
    xml->readNextStartElement();
    if (xml->name() == "PostSynapse"){
        Postsynapse *postsynpase;
        if (info->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC)
            postsynpase = parsePostsynapse(dst_pop_size);
        else if (info->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_DST)
            postsynpase = parsePostsynapse(neuron->size);
        target->postsynapse = postsynpase;
    } else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected 'PostSynapse' element in Target instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    xml->skipCurrentElement(); //jump out of projection element
    return target;
}

WeightUpdate *Parser::parseSynapse(uint synapse_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "WeightUpdate");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Synapse on line " << xml->lineNumber();

    WeightUpdate *weight_update = new WeightUpdate();

    //attributes
    weight_update->name = Parser::getStringAttribute(xml, "name");
    weight_update->definition_url = Parser::getStringAttribute(xml, "url");
    weight_update->input_src_port = Parser::getStringAttribute(xml, "input_src_port");
    weight_update->input_dst_port = Parser::getStringAttribute(xml, "input_dst_port");


    //parse inputs and properties
    while (xml->readNextStartElement()) {
        if (xml->name() == "Property"){
            weight_update->properties.append(parseProperty(synapse_size));
        }else if (xml->name() == "Input"){
            //not supported - test performed by info parser
            /*
            Input *input = parseInput(weight_update, synapse_size);
            QString src = "%1_%2_%3";
            src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
            weight_update->inputs[src] = input;
            */
        }
        else
            xml->skipCurrentElement();
    }

    return weight_update;
}

Postsynapse *Parser::parsePostsynapse(uint postsynapse_size)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "PostSynapse");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found PostSynapse on line " << xml->lineNumber();

    Postsynapse *postsynapse = new Postsynapse();

    //attributes
    postsynapse->name = Parser::getStringAttribute(xml, "name");
    postsynapse->definition_url = Parser::getStringAttribute(xml, "url");
    postsynapse->input_src_port = Parser::getStringAttribute(xml, "input_src_port");
    postsynapse->input_dst_port = Parser::getStringAttribute(xml, "input_dst_port");
    postsynapse->output_src_port = Parser::getStringAttribute(xml, "output_src_port");
    postsynapse->output_dst_port = Parser::getStringAttribute(xml, "output_dst_port");


    //parse inputs and properties
    while (xml->readNextStartElement()) {
        if (xml->name() == "Property"){
            postsynapse->properties.append(parseProperty(postsynapse_size));
        }else if (xml->name() == "Input"){
            Input *input = parseInput(postsynapse, postsynapse_size);
            QString src = "%1_%2_%3";
            src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
            postsynapse->inputs[src] = input;
        }
        else
            xml->skipCurrentElement();
    }


    return postsynapse;
}

PropertyValue *Parser::parseDelayPropertyValue()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Delay");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Delay on line " << xml->lineNumber();

    PropertyValue* value = NULL;

    xml->readNextStartElement();
    if (xml->name() == "FixedValue"){
        FixedPropertyValue *prop_fixed = new FixedPropertyValue();
        prop_fixed->value = Parser::getDoubleAttribute(xml, "value");
        value = (PropertyValue*)prop_fixed;

    }else  if (xml->name() == "UniformDistribution"){
        UniformDistPropertyValue *uniform_dist_value = new UniformDistPropertyValue();
        uniform_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        uniform_dist_value->minimum = Parser::getDoubleAttribute(xml, "minimum");
        uniform_dist_value->maximum = Parser::getDoubleAttribute(xml, "maximum");
        value = (PropertyValue*)uniform_dist_value;
    }else if(xml->name() == "NormalDistribution"){
        NormalDistPropertyValue *normal_dist_value = new NormalDistPropertyValue();
        normal_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        normal_dist_value->mean = Parser::getDoubleAttribute(xml, "mean");
        normal_dist_value->variance = Parser::getDoubleAttribute(xml, "variance");
        value = (PropertyValue*)normal_dist_value;
    }else if(xml->name() == "PoissonDistribution"){
        PoissonDistPropertyValue *poisson_dist_value = new PoissonDistPropertyValue();
        poisson_dist_value->seed = Parser::getIntAttribute(xml, "seed", true);
        poisson_dist_value->mean = Parser::getDoubleAttribute(xml, "mean");
        value = (PropertyValue*)poisson_dist_value;
    }else {
        std::cerr << "Error (line " << xml->lineNumber() << "): Expected a FixedValue or Ditribution type element in Delay instead of '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
        exit(0);
    }

    xml->skipCurrentElement();  //skip out of value
    xml->skipCurrentElement();  //skip out of delay

    return value;
}

Experiment *Parser::parseExperiment(QString input_path)
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "Experiment");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found Experiment on line " << xml->lineNumber();

    Experiment* experiment = new Experiment();

    //parse inputs and properties
    while (xml->readNextStartElement()) {
        if (xml->name() == "Model"){
            QString network_layer = Parser::getStringAttribute(xml, "network_layer_url");
            QString network_layer_abs = "%1/%2";
            network_layer_abs = network_layer_abs.arg(input_path).arg(network_layer);
            experiment->network_layer_url = network_layer_abs;
            xml->skipCurrentElement(); //skip Model
        }else if (xml->name() == "Simulation"){
            experiment->duration = Parser::getDoubleAttribute(xml, "duration");
            xml->readNextStartElement();
            if (xml->name() != "EulerIntegration"){
                std::cerr << "Error (Experiment line " << xml->lineNumber() << "): Only EulerIntegration supported. Found '" <<  xml->name().toString().toLocal8Bit().data() << "'" << std::endl;
                exit(0);
            }
            experiment->time_step = Parser::getDoubleAttribute(xml, "dt");
            xml->skipCurrentElement(); //skip out of EulerInteration
            xml->skipCurrentElement(); // skip out of Simulation
        }else if (xml->name() == "LogOutput"){
            LogOutput* log = parseLogOutput();
            experiment->outputs.insertMulti(log->target, log);
        }
        else{
            std::cerr << "Experiment Warning (line " << xml->lineNumber() << "): Element '" <<  xml->name().toString().toLocal8Bit().data() << "' not supported by splitter!" << std::endl;
            exit(0);
        }
    }
    return experiment;
}

LogOutput *Parser::parseLogOutput()
{
    //sanity check
    Q_ASSERT(xml->isStartElement() && xml->name() == "LogOutput");
    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "Parser: Found LogOutput on line " << xml->lineNumber();

    LogOutput* log = new LogOutput();

    log->name = Parser::getStringAttribute(xml, "name");
    log->target = Parser::getStringAttribute(xml, "target");
    log->port = Parser::getStringAttribute(xml, "port");
    log->start_time = Parser::getDoubleAttribute(xml, "start_time", true);
    log->end_time = Parser::getDoubleAttribute(xml, "end_time", true);
    QString indices = Parser::getStringAttribute(xml, "indices", true);
    if (indices != ""){
        QStringList indices_list = indices.split(",");
        for(int i=0; i<indices_list.size(); i++){
            log->indices.insert(indices_list[i].toInt());
        }
    }
    xml->skipCurrentElement(); //skip LogOutput
    return log;

}




/* Static functions */

QString Parser::readStringElement(QXmlStreamReader *xml)
{
    QString element = xml->name().toString();
    QString value = xml->readElementText();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read xml string value " << value << " in element '" << element << "'";
    return value;
}

int Parser::readIntElement(QXmlStreamReader *xml)
{
    QString element = xml->name().toString();
    int value = xml->readElementText().toInt();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read xml int value " << value << " in element '" << element << "'";
    return value;
}

double Parser::readDoubleElement(QXmlStreamReader *xml)
{
    QString element = xml->name().toString();
    double value = xml->readElementText().toDouble();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read xml double value " << value << " in element '" << element << "'";
    return value;
}

QString Parser::getStringAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional)
{
    QString element = xml->name().toString();
    if (!xml->attributes().hasAttribute(attribute_name)){
        if (optional)
            return QString();
        std::cerr << "Error: Attribute '" << attribute_name.toLocal8Bit().data() << "' not found in element '" << element.toLocal8Bit().data() << "'!" << std::endl;
        exit(0);
    }
    QString value = xml->attributes().value(attribute_name).toString();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read "<< attribute_name.toLocal8Bit().data() << " xml attribute " << value.toLocal8Bit().data();
    return value;
}

int Parser::getIntAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional)
{
    QString element = xml->name().toString();
    if (!xml->attributes().hasAttribute(attribute_name)){
        if (optional)
            return 0;
        std::cerr << "Error: Attribute " << attribute_name.toLocal8Bit().data() << " not found in element '" << element.toLocal8Bit().data() << "'!" << std::endl;
        exit(0);
    }
    int value = xml->attributes().value(attribute_name).toString().toInt();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read "<< attribute_name.toLocal8Bit().data() << " xml attribute " << value;
    return value;
}

double Parser::getDoubleAttribute(QXmlStreamReader *xml, QString attribute_name, bool optional)
{
    QString element = xml->name().toString();
    if (!xml->attributes().hasAttribute(attribute_name)){
        if (optional)
            return 0;
        std::cerr << "Error: Attribute " << attribute_name.toLocal8Bit().data() << " not found in element " << element.toLocal8Bit().data() << "!" << std::endl;
        exit(0);
    }
    double value = xml->attributes().value(attribute_name).toString().toDouble();
    if (XML_READING_DEBUG_OUTPUT)
        qDebug() << "  Parser: Read "<< attribute_name.toLocal8Bit().data() << " xml attribute " << value;
    return value;
}

/* stats */


uint Parser::getParsedPopulationCount()
{
    return parsed_populations;
}

uint Parser::getParsedProjectionCount()
{
    return parsed_projections;
}

uint Parser::getParsedInputCount()
{
    return parsed_inputs;
}

