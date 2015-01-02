#include "splitter.h"
#include "xmlwriter.h"
#include "aliaswriter.h"
#include "graphwriter.h"

#include <QFile>
#include <QFileInfo>
#include <iostream>
#include <QDebug>
#include <omp.h>


SpineMLSplitter::SpineMLSplitter(bool parallel, bool formatted_output, bool silent, WriterMode mode)
{
    parser = NULL;
    this->parallel = parallel;
    this->formatted_output = formatted_output;
    this->silent = silent;
    this->mode = mode;
    split_time = 0;
    temp_time = 0;
    timer.start();
}

SpineMLSplitter::~SpineMLSplitter()
{
    //cleanup
    if (parser)
        delete parser;
    if (info_parser)
        delete info_parser;
    parser = NULL;
    info_parser = NULL;
}

void SpineMLSplitter::split(QString experiment_input_filename, QString network_output_filename)
{
    if (parser)
        delete parser;
    if (info_parser)
        delete info_parser;

    //reset counters
    split_populations  = 0;
    split_projections  = 0;
    split_inputs       = 0;

    //init
    info_parser = new InfoParser(&xml_src);
    parser = new Parser(&xml_src, info_parser);

    //experiment file parse
    parseExperimentFile(experiment_input_filename, network_output_filename);

    //parser and info parser deleted by destructor
}

uint SpineMLSplitter::getSplitPopulationCount()
{
    return split_populations;
}

uint SpineMLSplitter::getSplitProjectionCount()
{
    return split_projections;
}

uint SpineMLSplitter::getSplitInputCount()
{
    return split_inputs;
}



uint SpineMLSplitter::getSplitTime()
{
    return split_time;
}

uint SpineMLSplitter::getTotalTime()
{
    return timer.elapsed();
}

/************************** Private functions ********************************/

void SpineMLSplitter::parseExperimentNetwork(Experiment* experiment, QString network_output_filename)
{
    //xml input
    qDebug() << "network_layer_url: " << experiment->network_layer_url << " network_output_filename: " << network_output_filename;
    QFileInfo network_fileinfo(experiment->network_layer_url);

    //QString dstproj_network_filename = "%1/%2_dstproj.xml";
    QString dstproj_network_filename = "%1/%2.xml";
    dstproj_network_filename = dstproj_network_filename.arg(network_fileinfo.absolutePath()).arg(network_fileinfo.baseName());

    QFile input_file(dstproj_network_filename.toLocal8Bit().data());

    if (!input_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error opening network input file: " << dstproj_network_filename.toLocal8Bit().data() << std::endl;
        exit(0);
    }
    xml_src.setDevice(&input_file);

    //FIRST PASS PARSING: I.E. INFO PARSE
    info_parser->parse();

    //INIT OUTPUT
    switch(mode){
    case(WRITER_MODE_XML):{
            writer = new SpineMLXMLWriter(network_output_filename, formatted_output);
        break;
        }
        case(WRITER_MODE_ALIAS):{
            if (info_parser->getSplitterMode() != SPLITMODE_PROJ_DEF_AT_DST){
                std::cerr << "DAMSON Alias mode (-alias) can only be used for projections specified at destination!" << std::endl;
                exit(0);
            }
            writer = new DamsonAliasWriter(network_output_filename, info_parser, experiment);
            break;
        }
        case(WRITER_MODE_GRAPH):{
            writer = new GraphWriter(network_output_filename, info_parser);
            break;
        }
    }

    writer->writeDocumentStart();

    //SECOND PASS PARSING. I.E. FULL PARSE
    input_file.reset();
    xml_src.setDevice(&input_file);
    parseAndSplitPopulations();

    input_file.close();

    //end writing
    writer->writeDocuemntEnd();
    writer->close();

    //cleanup
    delete writer;
}


void SpineMLSplitter::parseExperimentFile(QString experiment_input_filename, QString network_output_filename)
{
    Experiment *experiment;

    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "*** Start Experiment Parsing";


    QFile input_file(experiment_input_filename.toLocal8Bit().data());
    QFileInfo input_info(input_file);

    if (!input_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error opening experiment input file: " << experiment_input_filename.toLocal8Bit().data()  << std::endl;
        exit(0);
    }
    xml_src.setDevice(&input_file);

    //get the first (hopefully only) Experiment
    xml_src.readNextStartElement(); //read first 'spineml' element
    qDebug() << "first spineml element: " << xml_src.name();
    xml_src.readNextStartElement(); //read Experiment element
    qDebug() << "next spineml element expected to be experiment: " << xml_src.name();
    if (xml_src.name() == "Experiment"){
        qDebug() << "Parse experiment at " << input_info.absolutePath();
        experiment = parser->parseExperiment(input_info.absolutePath());
    }
    xml_src.skipCurrentElement(); //skip Experiment
    if (xml_src.readNextStartElement()){
        std::cerr << "Warning (line " << xml_src.lineNumber() << "): Multiple 'Experiment' elements found. Experiment will be ignored!" << std::endl;
    }

    input_file.close();

    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "*** End Experiment Parsing";

    //parse and split the network
    parseExperimentNetwork(experiment, network_output_filename);
    delete experiment;

}

void SpineMLSplitter::parseAndSplitPopulations()
{

    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "*** Start Full Network Parsing";

    xml_src.readNextStartElement(); //read first 'spineml' element
    while (xml_src.readNextStartElement()) {
         if (xml_src.name() == "Population"){
             Population *population = parser->parsePopulation();
             //PERFORM SPLITTING
             splitPopulationExplicit(population, population->neuron->size);
             delete population;
         }
         //info parser already rejected groups
         else
             xml_src.skipCurrentElement();
    }

    if (PARSER_DEBUG_OUTPUT)
        qDebug() << "*** End Full Network Parsing";
}





/******************splitter****************/

void SpineMLSplitter::splitPopulation(Population *population, uint component_size)
{
    uint num_src_sub_comps = UINT_DIV_CEIL(component_size, MAX_POPULATION_SIZE);

    if(parallel){
        temp_time = timer.elapsed();

        //openmp threads
        int iCPU = omp_get_num_procs();
        omp_set_num_threads(iCPU);

        uint batches = UINT_DIV_CEIL(num_src_sub_comps, iCPU);
        for(uint i=0; i<batches; i++){
            //number of sub components in batch
            uint batch_sub_comps = iCPU;
            if ((i+1) == batches){
                batch_sub_comps = num_src_sub_comps % iCPU;
                if (batch_sub_comps == 0)
                    batch_sub_comps = iCPU;
            }

            Population *sub_pops = new Population[batch_sub_comps];
            #pragma omp parallel for
            for(uint j=0; j<batch_sub_comps;j++)
            {
                //SPLIT
                uint sub_pop_index = j + (i*iCPU);
                #pragma omp atomic
                ++split_populations;
                Population *sub_pop = &sub_pops[j];
                sub_pop->neuron = new Neuron();
                splitNeuron(population->neuron, sub_pop->neuron, sub_pop_index, num_src_sub_comps);
                splitProjections(population, sub_pop, sub_pop_index);
                if (!silent)
                    qDebug() << "Split " << population->neuron->name << " sub " << sub_pop_index;
            }
            split_time += timer.elapsed() - temp_time;
            for(uint j=0; j<batch_sub_comps;j++)        //WRITE
            {
                uint sub_pop_index = j + (i*iCPU);
                writer->writePopulation((Population*)&sub_pops[j], population);
                if (!silent)
                    qDebug() << "Written " << (&sub_pops[j])->neuron->name << " sub " << sub_pop_index;
            }
            delete [] sub_pops;

        }
    }
    else    //NON OPENMP
    {
        for(uint i=0; i<num_src_sub_comps;i++)
        {
            temp_time = timer.elapsed();

            //SPLIT POPULATION
            split_populations++;
            Population *sub_pop = new Population();
            sub_pop->neuron = new Neuron();
            splitNeuron(population->neuron, sub_pop->neuron, i, num_src_sub_comps);
            splitProjections(population, sub_pop, i);
            split_time += timer.elapsed() - temp_time;
            writer->writePopulation(sub_pop, population);   //OUTPUT sub population
            delete sub_pop;             //CLEANUP
            if (!silent)
                qDebug() << "Split and Written " << population->neuron->name << " sub " << i;
        }
    }
}

void SpineMLSplitter::splitPopulationExplicit(Population *population, uint component_size)
{
    //cant write split projections or inputs until the maximum synapse split sizes have been calculated and stored in the unplit synapse!
    uint num_src_sub_comps = UINT_DIV_CEIL(component_size, MAX_POPULATION_SIZE);
    Population *sub_pops = new Population[num_src_sub_comps];

    if(parallel){
        temp_time = timer.elapsed();

        //openmp threads
        int iCPU = omp_get_num_procs();
        omp_set_num_threads(iCPU);

        uint batches = UINT_DIV_CEIL(num_src_sub_comps, iCPU);
        for(uint i=0; i<batches; i++){
            //number of sub components in batch
            uint batch_sub_comps = iCPU;
            if ((i+1) == batches){
                batch_sub_comps = num_src_sub_comps % iCPU;
                if (batch_sub_comps == 0)
                    batch_sub_comps = iCPU;
            }

            #pragma omp parallel for
            for(uint j=0; j<batch_sub_comps;j++)
            {
                //SPLIT
                uint sub_pop_index = j + (i*iCPU);
                #pragma omp atomic
                ++split_populations;
                Population *sub_pop = &sub_pops[sub_pop_index];
                sub_pop->neuron = new Neuron();
                splitNeuron(population->neuron, sub_pop->neuron, sub_pop_index, num_src_sub_comps);
                splitProjections(population, sub_pop, sub_pop_index);
                if (!silent)
                    qDebug() << "Split " << population->neuron->name << " sub " << sub_pop_index;
            }
            split_time += timer.elapsed() - temp_time;
        }
    }
    else    //NON OPENMP
    {
        for(uint i=0; i<num_src_sub_comps;i++)
        {
            temp_time = timer.elapsed();

            //SPLIT POPULATION
            split_populations++;
            Population *sub_pop = &sub_pops[i];
            sub_pop->neuron = new Neuron();
            splitNeuron(population->neuron, sub_pop->neuron, i, num_src_sub_comps);
            splitProjections(population, sub_pop, i);
            split_time += timer.elapsed() - temp_time;
            if (!silent)
                qDebug() << "Split " << population->neuron->name << " sub " << i;
        }
    }

    //write
    for(uint i=0; i<num_src_sub_comps;i++)
    {
        writer->writePopulation((Population*)&sub_pops[i], population);
        if (!silent)
            qDebug() << "Written " << (&sub_pops[i])->neuron->name << " sub " << i;
    }
    delete [] sub_pops;
}


void SpineMLSplitter::splitNeuron(Neuron *neuron, Neuron *sub_neuron, uint sub_pop_index, uint sub_pop_count)
{
    sub_neuron->name = getSubName(neuron->name, sub_pop_index);
    sub_neuron->definition_url = neuron->definition_url;
    if (sub_pop_index==(sub_pop_count-1)){         //if last sub population then it may be a part population
        uint remainder = neuron->size % MAX_POPULATION_SIZE;
        if (remainder != 0)
            sub_neuron->size = remainder;
        else
            sub_neuron->size = MAX_POPULATION_SIZE;
    }else
        sub_neuron->size = MAX_POPULATION_SIZE;
    if(SPLITTER_DEBUG_OUTPUT)
        qDebug() << "Splitter: New Sub Population " << sub_neuron->name << " size=" << sub_neuron->size;

    //properties
    splitProperties(neuron, sub_neuron, sub_pop_index, sub_neuron->size);

    //inputs
    splitInputs(neuron, sub_neuron, sub_pop_index, sub_neuron->size);

}

void SpineMLSplitter::splitInputs(Component *component, Component *sub_component, uint sub_comp_index, uint sub_comp_size)
{
    //inputs
    //TODO: input name is now src_x where x is the source sub index. This needs testing!!
    for (int i=0; i<component->inputs.values().size(); i++)
    {
        Input *input = component->inputs.values()[i];
        ComponentInfo *comp_info = info_parser->getComponentInfo(input->src);  //cant be NULL as parse has checked this in parseInput function

        switch(input->remapping->Type()){
            case(ONE_TO_ONE_CONNECTVITY_TYPE):{                //not supported for synapse or postsynapse (checked by parser)
                QString src = "%1_%2_%3";
                src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
                QString src_unique_name = getSubName(src, sub_comp_index);          //sub_comp_index == src_sub_index
                QString src_sub_comp_name = getSubName(input->src, sub_comp_index); //sub_comp_index == src_sub_index
                uint sub_input_index = 0;
                getSubInput(input, sub_component, src_unique_name, src_sub_comp_name, sub_input_index);
                input->sub_inp_max = 1;
                break;
            }
            case(ALL_TO_ALL_CONNECTVITY_TYPE):
            case(FIXED_PROBABILITY_CONNECTVITY_TYPE):{
                //sub input for each sub population/componenet
                uint sub_inputs = UINT_DIV_CEIL(comp_info->size, MAX_POPULATION_SIZE);
                for (uint i=0; i< sub_inputs;i++){
                    QString src = "%1_%2_%3";
                    src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
                    QString src_unique_name = getSubName(src, i);
                    QString src_sub_comp_name = getSubName(input->src, i);
                    uint sub_input_index = i;
                    getSubInput(input, sub_component, src_unique_name, src_sub_comp_name, sub_input_index);
                }
                input->sub_inp_max = sub_inputs;
                break;
            }
            case(LIST_CONNECTVITY_TYPE):{
                ConnectionList *connection_list = (ConnectionList*)input->remapping;
                uint sub_input_count = 0;

                int max_comp_size = 0;                                              //maximum number of component indices for remapping destination
                switch(component->Type()){
                    case(COMPONENT_TYPE_POPULATION):
                    case(COMPONENT_TYPE_POSTSYNAPSE):{
                        max_comp_size = MAX_POPULATION_SIZE;
                        break;
                    }
                    case(COMPONENT_TYPE_WEIGHT_UPDATE):{
                        //not supported
                        break;
                    }
                }

                int dst_index_start = sub_comp_index*max_comp_size;
                int dst_index_end = dst_index_start + sub_comp_size;
                for (int n=dst_index_start; n<dst_index_end; n++){

                    if (connection_list->connectionMatrix.contains(n)){
                        QList <ConnectionInstance*> connections = connection_list->connectionMatrix[n].values();
                        //check connection instances to see if this target is required for the sub projection
                        for(int c=0;c<connections.size();c++)
                        {
                            ConnectionInstance *inst = connections[c];
                            uint d = inst->src_neuron/MAX_POPULATION_SIZE;                  //sub componenent number of src neuron
                            ConnectionInstance *sub_inst = new ConnectionInstance();
                            sub_inst->delay = inst->delay;
                            sub_inst->src_neuron = inst->src_neuron % MAX_POPULATION_SIZE;  //always resize in neuron space (as only comp inst and populations are valid src)
                            sub_inst->dst_neuron = inst->dst_neuron % max_comp_size;        //resize by maximum comp size
                            //get sub input (either existing or new)
                            QString src = "%1_%2_%3";
                            src = src.arg(input->src).arg(input->src_port).arg(input->dst_port);
                            QString src_unique_name = getSubName(src, d);
                            QString src_sub_comp_name = getSubName(input->src, d);
                            Input *sub_input = getSubInput(input, sub_component, src_unique_name, src_sub_comp_name, sub_input_count);
                            if (sub_input->remapping->Type() != LIST_CONNECTVITY_TYPE){ //should never happen!
                                std::cerr << "Error: Sub input remapping type missmatch" << std::endl;
                                exit(0);
                            }
                            ConnectionList *sub_connection_list = (ConnectionList*)sub_input->remapping;
                            sub_inst->index = sub_connection_list->connectionIndices.values().size();                     //re-index
                            sub_connection_list->connectionIndices[sub_inst->index] = sub_inst;
                            sub_connection_list->connectionMatrix[sub_inst->dst_neuron].insertMulti(sub_inst->src_neuron, sub_inst);
                        }
                    }
                }

                //update max sub input count
                if (sub_input_count > input->sub_inp_max){
                    input->sub_inp_max = sub_input_count;
                }
                break;
            }
            case(NULL_CONNECTIVITY_TYPE):{
                break;//not possible checked by parser
            }
        }
    }
}


void SpineMLSplitter::splitProjections(Population *population, Population *sub_pop, uint sub_pop_index)
{
    for (int p=0;p<population->projections.values().size();p++){
        Projection *projection = population->projections.values()[p];

        //check src or dst name
        if (!info_parser->componentExists(projection->proj_population)){
            std::cerr << "Error (line " << xml_src.lineNumber() << "): Projection source or destination '" << projection->proj_population.toLocal8Bit().data() << "' not found in model." << std::endl;
            exit(0);
        }
        ComponentInfo *target_info = info_parser->getComponentInfo(projection->proj_population);
        if (target_info->Type() != COMPONENT_TYPE_POPULATION)
        {
            std::cerr << "Error (line " << xml_src.lineNumber() << "): Projection source or destination '" << projection->proj_population.toLocal8Bit().data() << "' is not a popultion name." << std::endl;
            exit(0);
        }
        PopulationInfo * target_pop_info = (PopulationInfo*)target_info;

        //target is the named src or dst of the projection
        uint target_pop_size = target_pop_info->size;
        uint target_sub_pop_count = UINT_DIV_CEIL(target_pop_info->size, MAX_POPULATION_SIZE);

        for (int c=0;c<projection->synapses.values().size();c++)
        {
            Synapse* synapse = projection->synapses.values()[c];
            switch(synapse->connection->Type())
            {
                case(ALL_TO_ALL_CONNECTVITY_TYPE):
                {
                    //projection required for each target sub population
                    for(uint d=0;d<target_sub_pop_count; d++)
                    {
                        AllToAllConnection *all_to_all = (AllToAllConnection*)synapse->connection;
                        QString taregt_sub_pop_name = getSubName(projection->proj_population, d);
                        uint target_sub_pop_size = MAX_POPULATION_SIZE;
                        if (d == (target_sub_pop_count-1)){
                            uint r = target_pop_size % MAX_POPULATION_SIZE;
                            if (r != 0)
                                target_sub_pop_size = r;
                        }
                        Projection *sub_proj = getSubProjection(sub_pop, taregt_sub_pop_name);
                        Synapse *sub_synapse = new Synapse();
                        sub_synapse->unsplit_synapse = synapse;
                        sub_synapse->_sub_syn_index = d;

                        AllToAllConnection *sub_all_to_all = new AllToAllConnection();
                        sub_all_to_all->delay = cloneDelayPropertyValue(all_to_all->delay);
                        sub_synapse->connection = (AbstractionConnection*) sub_all_to_all;
                        splitWeightUpdate(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, population->neuron->size, d, target_sub_pop_size, target_pop_size);
                        splitPostsynapse(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, d, target_sub_pop_size);
                        sub_proj->synapses[sub_synapse->weightupdate->name] = sub_synapse;
                        if (SPLITTER_DEBUG_OUTPUT)
                            qDebug() << "Splitter: New Synapse (with all to all connection) added to Sub Projection (" << sub_pop->neuron->name << "->"<< taregt_sub_pop_name <<")";
                    }
                    synapse->_sub_syn_max = target_sub_pop_count;
                    break;
                }
                case(ONE_TO_ONE_CONNECTVITY_TYPE):
                {
                    OneToOneConnection *one_to_one = (OneToOneConnection*)synapse->connection;
                    //Check dimensionality of populatoins to make sure they can be connected
                    if (target_pop_size != population->neuron->size){
                        std::cerr << "Error: Population sizes must be equal in synapse with one to one connection between '" << population->neuron->name.toLocal8Bit().data() << "' and '" << projection->proj_population.toLocal8Bit().data() << "'." << std::endl;
                        exit(0);
                    }
                    //single projection required between sub populations
                    QString target_sub_pop_name = getSubName(projection->proj_population, sub_pop_index);
                    Projection *sub_proj = getSubProjection(sub_pop, target_sub_pop_name);
                    Synapse *sub_synapse = new Synapse();
                    sub_synapse->unsplit_synapse = synapse;
                    sub_synapse->_sub_syn_index = 0;
                    synapse->_sub_syn_max = 1;

                    OneToOneConnection *sub_one_to_one = new OneToOneConnection();
                    sub_one_to_one->delay = cloneDelayPropertyValue(one_to_one->delay);
                    sub_synapse->connection = (AbstractionConnection*) sub_one_to_one;
                    splitWeightUpdate(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, population->neuron->size, sub_pop_index, sub_pop->neuron->size, target_pop_size); //sub_pop_size = target_sub_pop_size
                    splitPostsynapse(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, sub_pop_index, sub_pop->neuron->size);
                    sub_proj->synapses[sub_synapse->weightupdate->name] = sub_synapse;
                    if (SPLITTER_DEBUG_OUTPUT)
                        qDebug() << "Splitter: New Synapse (with one to one connection) added to Sub Projection (" << sub_pop->neuron->name << "->"<< target_sub_pop_name <<")";

                    break;
                }
                case(LIST_CONNECTVITY_TYPE):
                {
                    //check connectivity instances list and make projection accordingly
                    ConnectionList *connection_list = (ConnectionList*)synapse->connection;
                    uint sub_pop_index_start = sub_pop_index*MAX_POPULATION_SIZE;
                    uint sub_pop_index_end = sub_pop_index_start + sub_pop->neuron->size;

                    uint sub_synapse_count = 0;
                    for(uint n=sub_pop_index_start;n<sub_pop_index_end;n++)
                    {
                        //if source neuron has projections to other neurons in dst pop
                        if (connection_list->connectionMatrix.contains(n))
                        {
                            QList <ConnectionInstance*> connections = connection_list->connectionMatrix[n].values();
                            //check connection instances to see if this target is required for the sub projection
                            for(int c=0;c<connections.size();c++)
                            {
                                ConnectionInstance *inst = connections[c];
                                uint d = inst->dst_neuron/MAX_POPULATION_SIZE;          //sub population number of dst neuron
                                ConnectionInstance *sub_inst = new ConnectionInstance();
                                sub_inst->delay = inst->delay;
                                sub_inst->src_neuron = inst->src_neuron % MAX_POPULATION_SIZE;;
                                sub_inst->dst_neuron = inst->dst_neuron % MAX_POPULATION_SIZE;

                                QString target_sub_pop_name = getSubName(projection->proj_population, d);
                                Projection *sub_proj = getSubProjection(sub_pop, target_sub_pop_name);

                                //if synapse is not already within the projection then add it
                                QString sub_wu_name = "%1_sub%2_%3";
                                sub_wu_name = sub_wu_name.arg(synapse->weightupdate->name).arg(sub_pop_index).arg(d);
                                Synapse * sub_synapse = NULL;
                                if (sub_proj->synapses.contains(sub_wu_name)){
                                    sub_synapse = sub_proj->synapses[sub_wu_name];
                                }else{
                                    //new synapse! split wu and ps later (requires all sub connectivity to be calculated first)
                                    sub_synapse = new Synapse();
                                    sub_synapse->unsplit_synapse = synapse;
                                    sub_synapse->_sub_syn_index = sub_synapse_count++;
                                    sub_synapse->_sub_target_index = d;
                                    ConnectionList *sub_connection_list = new ConnectionList();
                                    sub_synapse->connection = (AbstractionConnection*) sub_connection_list;
                                    sub_connection_list->delay = cloneDelayPropertyValue(connection_list->delay);
                                    sub_proj->synapses[sub_wu_name] = sub_synapse; //update hash map
                                    if (SPLITTER_DEBUG_OUTPUT)
                                        qDebug() << "Splitter: New Synapse (with list connection) added to Sub Projection (" << sub_pop->neuron->name << "->"<< target_sub_pop_name <<")";
                                }
                                ConnectionList *sub_connection_list = (ConnectionList*)(sub_synapse->connection);
                                sub_inst->index = sub_connection_list->connectionIndices.values().size();
                                sub_connection_list->connectionIndices[sub_inst->index] = sub_inst;
                                if (sub_connection_list->connectionMatrix[sub_inst->src_neuron].contains(sub_inst->dst_neuron)){
                                    std::cerr << "Error: duplicate connection found from " << sub_pop->neuron->name.toLocal8Bit().data() << " index " << sub_inst->src_neuron << " to " << target_sub_pop_name.toLocal8Bit().data() << " index " << sub_inst->dst_neuron << std::endl;
                                    break;
                                }
                                sub_connection_list->connectionMatrix[sub_inst->src_neuron][sub_inst->dst_neuron] = sub_inst;
                                if (SPLITTER_DEBUG_OUTPUT)
                                    qDebug() << "Splitter: New Connection Instance from " << sub_pop->neuron->name << " index " << sub_inst->src_neuron << " to " << target_sub_pop_name << " index " << sub_inst->dst_neuron;
                            }

                        }
                    }

                    //split WeightUpdate and PostSynapse
                    for (int p=0; p<sub_pop->projections.values().size(); p++){
                        Projection* sub_proj = sub_pop->projections.values().at(p);
                        for (int s=0; s<sub_proj->synapses.values().size(); s++){
                            Synapse* sub_synapse = sub_proj->synapses.values().at(s);
                            //get sub pop index of target
                            int d = sub_synapse->_sub_target_index;

                            //calculate target sub population size
                            uint target_sub_pop_size = MAX_POPULATION_SIZE;
                            if (d == (target_sub_pop_count-1)){
                                uint r = target_pop_size % MAX_POPULATION_SIZE;
                                if (r != 0)
                                    target_sub_pop_size = r;
                            }

                            splitWeightUpdate(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, population->neuron->size, d, target_sub_pop_size, target_pop_size);
                            splitPostsynapse(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, d, target_sub_pop_size);

                        }
                    }


                    //update the maximum sub synapse count for the unsplit synapse
                    if (sub_synapse_count > synapse->_sub_syn_max){
                        //qDebug() <<  "NEW MAX SUB SYN" << sub_synapse_count;
                        synapse->_sub_syn_max = sub_synapse_count;
                    }

                    break;
                }
                case(FIXED_PROBABILITY_CONNECTVITY_TYPE):
                {
                    FixedProbabilityConnection *fixed_prob_conn = (FixedProbabilityConnection*) synapse->connection;
                    //projection required for each dst sub population
                    for(uint d=0;d<target_sub_pop_count; d++)
                    {
                        QString target_sub_pop_name = getSubName(projection->proj_population, d);
                        //dst sub pop size
                        uint target_sub_pop_size = MAX_POPULATION_SIZE;
                        if (d == (target_sub_pop_count-1)){
                            uint r = target_pop_size % MAX_POPULATION_SIZE;
                            if (r != 0)
                                target_sub_pop_size = r;
                        }
                        Projection *sub_proj = getSubProjection(sub_pop, target_sub_pop_name);
                        Synapse *sub_synapse = new Synapse();
                        sub_synapse->unsplit_synapse = synapse;
                        sub_synapse->_sub_syn_index = d;

                        FixedProbabilityConnection *sub_fixed_prob_conn = new FixedProbabilityConnection();
                        sub_fixed_prob_conn->seed = fixed_prob_conn->seed;
                        sub_fixed_prob_conn->probability = fixed_prob_conn->probability;
                        sub_fixed_prob_conn->delay = cloneDelayPropertyValue(fixed_prob_conn->delay);
                        sub_synapse->connection = (AbstractionConnection*)sub_fixed_prob_conn;
                        splitWeightUpdate(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, population->neuron->size, d, target_sub_pop_size, target_pop_size);
                        splitPostsynapse(synapse, sub_synapse, sub_pop_index, sub_pop->neuron->size, d, target_sub_pop_size);
                        sub_proj->synapses[sub_synapse->weightupdate->name] = sub_synapse;
                        if (SPLITTER_DEBUG_OUTPUT)
                            qDebug() << "Splitter: New Synapse (with fixed probability connection) added to Sub Projection (" << sub_pop->neuron->name << "->"<< target_sub_pop_name <<")";
                    }
                    synapse->_sub_syn_max = target_sub_pop_count;
                    break;
                }
                default:
                {
                    std::cerr << "Error: Synapse connection type not supported in population named" << population->neuron->name.toLocal8Bit().data() << std::endl;
                    break;
                }
            }
        }
    }
}


void SpineMLSplitter::splitWeightUpdate(Synapse *synapse, Synapse *sub_synapse, uint sub_pop_index, uint sub_pop_size, uint pop_size, uint target_sub_pop_index, uint target_sub_pop_size, uint target_pop_size)
{
    sub_synapse->weightupdate = new WeightUpdate();
    QString name = "%1_sub%2_%3";
    name = name.arg(synapse->weightupdate->name).arg(sub_pop_index).arg(target_sub_pop_index);
    sub_synapse->weightupdate->name = name;
    sub_synapse->weightupdate->definition_url = synapse->weightupdate->definition_url;
    sub_synapse->weightupdate->input_dst_port = synapse->weightupdate->input_dst_port;
    sub_synapse->weightupdate->input_src_port = synapse->weightupdate->input_src_port;
    sub_synapse->weightupdate->target_connectivity = sub_synapse->connection;

    //properties and inputs
    if (info_parser->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC){
        splitProperties(synapse->weightupdate, sub_synapse->weightupdate, sub_pop_index, sub_pop_size, target_sub_pop_index, target_sub_pop_size, target_pop_size);
        //no support for inputs for weight updates
    }
    else{
        splitProperties(synapse->weightupdate, sub_synapse->weightupdate, target_sub_pop_index, target_sub_pop_size, sub_pop_index, sub_pop_size, pop_size);
        //no upport for inputs for weight updates
    }

    //inputs
    splitInputs(synapse->weightupdate, sub_synapse->weightupdate, sub_pop_index, sub_pop_size*target_sub_pop_size);

}

void SpineMLSplitter::splitPostsynapse(Synapse *synapse, Synapse *sub_synapse, uint sub_pop_index, uint sub_pop_size, uint target_sub_pop_index, uint target_sub_pop_size)
{
    sub_synapse->postsynapse = new Postsynapse();
    QString name = "%1_sub%2_%3";
    name = name.arg(synapse->postsynapse->name).arg(sub_pop_index).arg(target_sub_pop_index);
    sub_synapse->postsynapse->name = name;
    sub_synapse->postsynapse->definition_url = synapse->postsynapse->definition_url;
    sub_synapse->postsynapse->input_src_port = synapse->postsynapse->input_src_port;
    sub_synapse->postsynapse->input_dst_port = synapse->postsynapse->input_dst_port;
    sub_synapse->postsynapse->output_src_port = synapse->postsynapse->output_src_port;
    sub_synapse->postsynapse->output_dst_port = synapse->postsynapse->output_dst_port;

    //properties (swap target and sub pop indices and sized for projections specified at dst)
    if (info_parser->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_SRC){
        splitProperties(synapse->postsynapse, sub_synapse->postsynapse, sub_pop_index, sub_pop_size, target_sub_pop_index, target_sub_pop_size); //no sub_comp_size required
        splitInputs(synapse->postsynapse, sub_synapse->postsynapse, target_sub_pop_index, target_sub_pop_size); //TODO TEST
    }
    else{
        splitProperties(synapse->postsynapse, sub_synapse->postsynapse, target_sub_pop_index, target_sub_pop_size, sub_pop_index, sub_pop_size); //reverse src and dst
        splitInputs(synapse->postsynapse, sub_synapse->postsynapse, sub_pop_index, sub_pop_size); //TODO:TEST
    }
}

void SpineMLSplitter::splitProperties(Component *component, Component *sub_component, uint sub_comp_index, uint sub_comp_size, uint target_sub_pop_index, uint target_sub_pop_size, uint target_pop_size)
{
    //properties
    for (int i=0; i< component->properties.size(); i++)
    {
        Property *property = component->properties[i];
        Property *sub_prop = new Property();
        sub_prop->name = property->name;
        sub_prop->dimension = property->dimension;

        //switch property type and either add the property or make sure it is deleted if not needed
        switch(property->value->Type()){
            case(FIXED_VALUE_TYPE):
            {
                FixedPropertyValue *property_value = (FixedPropertyValue*)property->value;
                FixedPropertyValue *sub_prop_value = new FixedPropertyValue();
                sub_prop_value->value = property_value->value;
                sub_prop->value = (PropertyValue*)sub_prop_value;
                sub_component->properties.append(sub_prop);
                break;
            }
            case(VALUE_LIST_TYPE):
            {
                PropertyValueList *property_value = (PropertyValueList*)property->value;
                PropertyValueList *sub_prop_value = new PropertyValueList();
                //Switch by component type
                switch(component->Type()){
                    case(COMPONENT_TYPE_POPULATION):{
                        uint start_index = sub_comp_index*MAX_POPULATION_SIZE;
                        for (uint j=start_index;j<start_index+sub_comp_size;j++)
                        {
                            if (property_value->valueInstances.contains(j)){
                                PropertyValueInstance* prop_inst = property_value->valueInstances[j];
                                PropertyValueInstance* sub_prop_inst = new PropertyValueInstance();
                                sub_prop_inst->index = prop_inst->index % MAX_POPULATION_SIZE;          //remap to sub neuron
                                sub_prop_inst->value = prop_inst->value;
                                sub_prop_value->valueInstances[sub_prop_inst->index] = sub_prop_inst;
                            }
                        }
                        break;
                    }
                    case(COMPONENT_TYPE_WEIGHT_UPDATE):{
                        WeightUpdate *synapse = (WeightUpdate*)component;
                        WeightUpdate *sub_synapse = (WeightUpdate*)sub_component;
                        switch(synapse->target_connectivity->Type()){
                            case(ALL_TO_ALL_CONNECTVITY_TYPE):{
                                uint sub_pop_offset = sub_comp_index*(MAX_POPULATION_SIZE*target_pop_size);            //offset by total number of index items per sub_population to sub_projection
                                uint target_sub_pop_offset = target_sub_pop_index*MAX_POPULATION_SIZE;                 //offset by the sub_projection destination index

                                for (uint i=0;i<sub_comp_size;i++){                                                    //loop through source neurons
                                    uint neuron_offset = i*target_pop_size;                                            //offset by source neuron item
                                    for (uint j=0;j<target_sub_pop_size;j++)                                           //loop through dest neurons (i.e. ind. synapses)
                                    {
                                        uint index = sub_pop_offset+target_sub_pop_offset+neuron_offset+j;
                                        if (property_value->valueInstances.contains(index)){
                                           PropertyValueInstance* prop_inst = property_value->valueInstances[index];
                                           PropertyValueInstance* sub_prop_inst = new PropertyValueInstance();
                                           sub_prop_inst->index = j+(i*target_sub_pop_size);                           //remap to sub projection: (source neuron * dst_pop_size) + dst neuron
                                           sub_prop_inst->value = prop_inst->value;
                                           sub_prop_value->valueInstances[sub_prop_inst->index] = sub_prop_inst;
                                       }
                                    }
                                }
                                break;
                            }
                            case(ONE_TO_ONE_CONNECTVITY_TYPE):{
                                //one to one mapping of neuron index and connection index (already checked in split projection)
                                uint start_index = sub_comp_index*MAX_POPULATION_SIZE;            //sub_proj_dst_index = sub_population_index when connectivity is one to one
                                for (uint j=start_index;j<start_index+(MAX_POPULATION_SIZE);j++)
                                {
                                    if (property_value->valueInstances.contains(j)){
                                        PropertyValueInstance* prop_inst = property_value->valueInstances[j];
                                        PropertyValueInstance* sub_prop_inst = new PropertyValueInstance();
                                        sub_prop_inst->index = prop_inst->index % MAX_POPULATION_SIZE;          //remap to sub projection
                                        sub_prop_inst->value = prop_inst->value;
                                        sub_prop_value->valueInstances[sub_prop_inst->index] = sub_prop_inst;
                                    }
                                }
                                break;
                            }
                            case(LIST_CONNECTVITY_TYPE):{
                                ConnectionList *connection_list = (ConnectionList*)synapse->target_connectivity;
                                ConnectionList *sub_connection_list = (ConnectionList*)sub_synapse->target_connectivity;
                                int start_index;
                                int target_start_index;
                                if (info_parser->getSplitterMode() == SPLITMODE_PROJ_DEF_AT_DST){
                                    start_index = target_sub_pop_index*MAX_POPULATION_SIZE;
                                    target_start_index = sub_comp_index*MAX_POPULATION_SIZE;
                                }else{
                                    start_index = sub_comp_index*MAX_POPULATION_SIZE;
                                    target_start_index = target_sub_pop_index*MAX_POPULATION_SIZE;
                                }
                                for (int s=0; s<MAX_POPULATION_SIZE; s++){
                                    if (connection_list->connectionMatrix.contains(start_index+s)){
                                        for (int t=0; t<MAX_POPULATION_SIZE; t++){
                                            if (connection_list->connectionMatrix[start_index+s].contains(target_start_index+t)){
                                                ConnectionInstance *inst = connection_list->connectionMatrix[start_index+s][target_start_index+t];
                                                ConnectionInstance *sub_inst = sub_connection_list->connectionMatrix[s][t];

                                                if (property_value->valueInstances.contains(inst->index)){
                                                    PropertyValueInstance* prop_inst = property_value->valueInstances[inst->index];
                                                    PropertyValueInstance* sub_prop_inst = new PropertyValueInstance();
                                                    sub_prop_inst->index = sub_inst->index;
                                                    sub_prop_inst->value = prop_inst->value;
                                                    sub_prop_value->valueInstances[sub_prop_inst->index] = sub_prop_inst;
                                                }
                                            }
                                        }
                                    }
                                }

                                break;
                            }
                            default:{
                                std::cerr << "Error: Unsupported connection type used for synapse '" << synapse->name.toLocal8Bit().data()  << "' property value list in " << std::endl;
                                delete sub_prop_value;
                                exit(0);
                                break;
                            }
                        }
                        break;
                    }
                    case(COMPONENT_TYPE_POSTSYNAPSE):{
                        uint start_index = target_sub_pop_index*MAX_POPULATION_SIZE;
                        for (uint j=start_index;j<start_index+(target_sub_pop_size);j++)
                        {
                            if (property_value->valueInstances.contains(j)){
                                PropertyValueInstance* prop_inst = property_value->valueInstances[j];
                                PropertyValueInstance* sub_prop_inst = new PropertyValueInstance();
                                sub_prop_inst->index = prop_inst->index % MAX_POPULATION_SIZE;          //remap to dst sub neuron
                                sub_prop_inst->value = prop_inst->value;
                                sub_prop_value->valueInstances[sub_prop_inst->index] = sub_prop_inst;
                            }
                        }
                        break;
                    }
                }

                //check property instances to see if the property is valid for the sub component
                sub_prop->value = (PropertyValue*)sub_prop_value;
                if (sub_prop_value->valueInstances.size() > 0){
                    sub_component->properties.append(sub_prop);
                }else{
                    delete sub_prop; //forget the property no instances for sub component
                }

                break;
            }
            case(UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE):
            {

                UniformDistPropertyValue *property_value = (UniformDistPropertyValue*)property->value;
                UniformDistPropertyValue *sub_prop_value = new UniformDistPropertyValue();
                sub_prop_value->seed = property_value->seed;
                sub_prop_value->minimum = property_value->minimum;
                sub_prop_value->maximum = property_value->maximum;
                sub_prop->value = (PropertyValue*)sub_prop_value;
                sub_component->properties.append(sub_prop);
                break;
            }
            case(NORMAL_DISTRIBUTION_STOCHASTIC_TYPE):
            {
                NormalDistPropertyValue *property_value = (NormalDistPropertyValue*)property->value;
                NormalDistPropertyValue *sub_prop_value = new NormalDistPropertyValue();
                sub_prop_value->seed = property_value->seed;
                sub_prop_value->mean = property_value->mean;
                sub_prop_value->variance = property_value->variance;
                sub_prop->value = (PropertyValue*)sub_prop_value;
                sub_component->properties.append(sub_prop);
                break;
            }
            case(POISSON_DISTRIBUTION_STOCHASTIC_TYPE):
            {
                PoissonDistPropertyValue *property_value = (PoissonDistPropertyValue*)property->value;
                PoissonDistPropertyValue *sub_prop_value = new PoissonDistPropertyValue();
                sub_prop_value->seed = property_value->seed;
                sub_prop_value->mean = property_value->mean;
                sub_prop->value = (PropertyValue*)sub_prop_value;
                sub_component->properties.append(sub_prop);
                break;
            }
            default:
            {
                std::cerr << "Warning: Splitter found unknown property type" << std::endl;
                delete sub_prop; //this will delete sub_prop_value
                break;
            }
        }

    }
}

PropertyValue *SpineMLSplitter::cloneDelayPropertyValue(PropertyValue *delay)
{
    PropertyValue *delay_copy = NULL;
    if (delay == NULL)  //if connection list then there might not be a global delay value
        return delay_copy;
    switch (delay->Type()){
        case(FIXED_VALUE_TYPE):{
            FixedPropertyValue *fixed = (FixedPropertyValue*) delay;
            FixedPropertyValue *fixed_copy = new FixedPropertyValue();
            fixed_copy->value = fixed->value;
            delay_copy = (PropertyValue*)fixed_copy;
            break;
        }
        case(UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE):{
            UniformDistPropertyValue *uniform = (UniformDistPropertyValue*) delay;
            UniformDistPropertyValue *uniform_copy = new UniformDistPropertyValue();
            uniform_copy->minimum = uniform->minimum;
            uniform_copy->maximum = uniform->maximum;
            uniform_copy->seed = uniform->seed;
            delay_copy = (PropertyValue*)uniform_copy;
            break;
        }
        case(NORMAL_DISTRIBUTION_STOCHASTIC_TYPE):{
            NormalDistPropertyValue *normal = (NormalDistPropertyValue*) delay;
            NormalDistPropertyValue *normal_copy = new NormalDistPropertyValue();
            normal_copy->mean = normal->mean;
            normal_copy->variance = normal->variance;
            normal_copy->seed = normal->seed;
            delay_copy = (PropertyValue*)normal_copy;
            break;
        }
        case(POISSON_DISTRIBUTION_STOCHASTIC_TYPE):{
            PoissonDistPropertyValue *poisson = (PoissonDistPropertyValue*) delay;
            PoissonDistPropertyValue *poisson_copy = new PoissonDistPropertyValue();
            poisson_copy->mean = poisson->mean;
            poisson_copy->seed = poisson->seed;
            delay_copy = (PropertyValue*)poisson_copy;
            break;
        }
        default:{
            //not possible only these types are read by parser!
            break;
        }
    }
    return delay_copy;
}


Projection *SpineMLSplitter::getSubProjection(Population *sub_population, QString dst_sub_pop_name)
{
    //returns existing sub projection if it exists otherwise creates a new one
    if (sub_population->projections.contains(dst_sub_pop_name)){
        return sub_population->projections[dst_sub_pop_name];
    }else{
        #pragma omp atomic
        split_projections++;
        Projection *sub_proj = new Projection();
        sub_proj->proj_population = dst_sub_pop_name;
        sub_population->projections[dst_sub_pop_name] = sub_proj; //update hash map
        if (SPLITTER_DEBUG_OUTPUT)
            qDebug() << "Splitter: New Sub Projection " << sub_population->neuron->name << " and " << dst_sub_pop_name;
        return sub_proj;
    }
}

Input *SpineMLSplitter::getSubInput(Input *input, Component *sub_componenent, QString src_unique_name, QString src_sub_comp_name, uint &sub_input_count)
{
    if(sub_componenent->inputs.contains(src_unique_name)){
        return sub_componenent->inputs[src_unique_name];
    }else{
        #pragma omp atomic
        split_inputs++;
        Input *sub_input = new Input();

        sub_input->unsplit_input = input;
        sub_input->sub_inp_index = sub_input_count++;

        sub_input->dst_port = input->dst_port;
        sub_input->src_port = input->src_port;
        sub_input->src = src_sub_comp_name;
        if (sub_input->remapping != NULL)
            sub_input->remapping->delay = cloneDelayPropertyValue(input->remapping->delay);

        switch(input->remapping->Type()){
            case(ONE_TO_ONE_CONNECTVITY_TYPE):{
                OneToOneConnection *sub_one_to_one = new OneToOneConnection();
                sub_input->remapping = (AbstractionConnection*)sub_one_to_one;
                break;
            }
            case(ALL_TO_ALL_CONNECTVITY_TYPE):{
                AllToAllConnection *sub_all_to_all = new AllToAllConnection();
                sub_input->remapping = (AbstractionConnection*)sub_all_to_all;
                break;
            }
            case(FIXED_PROBABILITY_CONNECTVITY_TYPE):{
                FixedProbabilityConnection *fixed = (FixedProbabilityConnection*)input->remapping;
                FixedProbabilityConnection *sub_fixed = new FixedProbabilityConnection();
                sub_fixed->seed = fixed->seed;
                sub_fixed->probability = fixed->probability;
                sub_input->remapping = (AbstractionConnection*)sub_fixed;
                break;
            }
            case(LIST_CONNECTVITY_TYPE):{
                ConnectionList *sub_connection_list = new ConnectionList();
                sub_input->remapping = (AbstractionConnection*)sub_connection_list;
                break;
            }
            default:{
                std::cerr << "Error: Splitter internal erorr" << std::endl;
                exit(0);
            }
        }
        //clone delay
        sub_input->remapping->delay = cloneDelayPropertyValue(input->remapping->delay);
        //add to sub componenet
        sub_componenent->inputs[src_unique_name] = sub_input;
        return sub_input;
    }
}

Parser *SpineMLSplitter::getParser()
{
    return parser;
}





QString SpineMLSplitter::getSubName(QString parent_name, uint sub_index)
{
    QString name = "%1_sub%2";
    name = name.arg(parent_name).arg(sub_index);
    return name;
}
