#ifndef MODELOBJECTS_H
#define MODELOBJECTS_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QMap>
#include <QSet>


/* forward declarations */

class PopulationInfo;
class Component;
class Population;
class Neuron;
class Property;
class PropertyValue;
class Input;
class Projection;
class Synapse;
class WeightUpdate;
class Postsynapse;
class AbstractionConnection;


/* typedefs */

typedef enum
{
    NULL_CONNECTIVITY_TYPE,
    ALL_TO_ALL_CONNECTVITY_TYPE,
    ONE_TO_ONE_CONNECTVITY_TYPE,
    LIST_CONNECTVITY_TYPE,
    FIXED_PROBABILITY_CONNECTVITY_TYPE
}ConnectivityType;

typedef enum
{
    COMPONENT_TYPE_POPULATION,
    COMPONENT_TYPE_WEIGHT_UPDATE,
    COMPONENT_TYPE_POSTSYNAPSE
}ComponentType;



typedef enum
{
    NULL_VALUE_TYPE,
    FIXED_VALUE_TYPE,
    VALUE_LIST_TYPE,
    UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE,
    NORMAL_DISTRIBUTION_STOCHASTIC_TYPE,
    POISSON_DISTRIBUTION_STOCHASTIC_TYPE
}PropertyValueType;




/* Component Info Classes - for information parsing stage */

class ComponentInfo
{
public:
    ComponentInfo(){ size = 0;}
    virtual ComponentType Type() = 0;
    virtual void calculateDimensions(QHash<QString, ComponentInfo *> &component_info) = 0;
public:
    QString name;
    uint size;
};


class PopulationInfo: public ComponentInfo
{
public:
    PopulationInfo(){}
    ComponentType Type(){return COMPONENT_TYPE_POPULATION;}
    void calculateDimensions(QHash<QString, ComponentInfo *> &component_info);
public:
    uint global_index;
    uint global_sub_start_index;
    uint splits;
};

class WeightUpdateInfo: public ComponentInfo
{
public:
    WeightUpdateInfo(){}
    ComponentType Type(){return COMPONENT_TYPE_WEIGHT_UPDATE;}
    void calculateDimensions(QHash<QString, ComponentInfo *> &component_info);
public:
    QString projPopulation;
    uint srcPopSize;
    uint dstPopSize;
    ConnectivityType connectivity;
    uint connectionListCount; //only used when ConnectivityType == LIST
};

class PostsynapseInfo: public ComponentInfo
{
public:
    PostsynapseInfo(){}
    ComponentType Type(){return COMPONENT_TYPE_POSTSYNAPSE;}
    void calculateDimensions(QHash<QString, ComponentInfo *> &component_info);
public:
    QString projPopulation;

};


/* Component Classes - for full parsing stage */

class Component
{
public:
    Component(){}
    ~Component();
    virtual ComponentType Type() = 0;
public:
    QString name;
    QString definition_url;
    QVector <Property*> properties;
    QHash <QString, Input*> inputs;
};


class Population
{
public:
    Population();
    ~Population();

public:
    Neuron* neuron;
    uint sub_pop_index;
    QHash <QString, Projection*> projections; //< dst_sub_population_name, subprojection >
};

class Neuron: public Component
{
public:
    Neuron(){}
    ComponentType Type(){return COMPONENT_TYPE_POPULATION;}
public:
    uint size;
};

class Property
{
public:
    Property();
    ~Property();
public:
    QString name;
    PropertyValue *value;
    QString dimension;
};

class PropertyValue
{
public:
    virtual ~PropertyValue(){}
public:
    virtual PropertyValueType Type() = 0;
};

class FixedPropertyValue: public PropertyValue
{
public:
    FixedPropertyValue(){}
    PropertyValueType Type(){return FIXED_VALUE_TYPE;}
public:
    double value;
};

class PropertyValueInstance
{
public:
    PropertyValueInstance(){}
public:
    uint index;
    double value;
};

class PropertyValueList: public PropertyValue
{
public:
    PropertyValueList(){}
    ~PropertyValueList();
    PropertyValueType Type(){return VALUE_LIST_TYPE;}
public:
    QHash <int, PropertyValueInstance*> valueInstances; // <index, value instance>
};


class UniformDistPropertyValue: public PropertyValue
{
public:
    UniformDistPropertyValue(){}
    PropertyValueType Type(){return UNIFORM_DISTRIBUTION_STOCHASTIC_TYPE;}
public:
    int seed;
    double minimum;
    double maximum;
};

class NormalDistPropertyValue: public PropertyValue
{
public:
    NormalDistPropertyValue(){}
    PropertyValueType Type(){return NORMAL_DISTRIBUTION_STOCHASTIC_TYPE;}
public:
    int seed;
    double mean;
    double variance;
};

class PoissonDistPropertyValue: public PropertyValue
{
public:
    PoissonDistPropertyValue(){}
    PropertyValueType Type(){return POISSON_DISTRIBUTION_STOCHASTIC_TYPE;}
public:
    int seed;
    double mean;
};

class Input
{
public:
    Input();
    ~Input();
public:
    QString src;
    QString src_port;
    QString dst_port;
    AbstractionConnection *remapping;
public:
    Input * unsplit_input;
    uint sub_inp_max;            //stores (in unsplit input) the max number of sub inputs for the in any of split populations
    uint sub_inp_index;          //stores (in sub synapse) the sub synapse index
    uint unsplit_index;          //stores (for unsplit inputs) the index of the inputs (with respect to the parent component)
};

class Projection
{
public:
    Projection(){}
    ~Projection();
public:
    QString proj_population;
    QHash<QString, Synapse*> synapses;  //<synapse name used as id, target>
    uint index;                         //unsplit index of projection in population
};

class Synapse
{
public:
    Synapse();
    ~Synapse();
public:
    AbstractionConnection *connection;
    WeightUpdate *weightupdate;
    Postsynapse *postsynapse;
public:
    Synapse * unsplit_synapse;
    uint _sub_syn_max;            //stores (in unsplit synapse) the max number of sub synapses for any split populations
    uint _sub_syn_index;          //stores (in sub synapse) the sub synapse index
    uint _sub_target_index;       //sub target index only for splits
};

class WeightUpdate: public Component
{
public:
    WeightUpdate(){}
    ComponentType Type(){return COMPONENT_TYPE_WEIGHT_UPDATE;}
    static QString unsplitName(QString split_name);
public:
    AbstractionConnection *target_connectivity; //not owned by object
    QString input_src_port;
    QString input_dst_port;
};

class Postsynapse: public Component
{
public:
    Postsynapse(){}
    ComponentType Type(){return COMPONENT_TYPE_POSTSYNAPSE;}
public:
    QString input_src_port;
    QString input_dst_port;
    QString output_src_port;
    QString output_dst_port;
};

class AbstractionConnection
{
public:
    AbstractionConnection();
    ~AbstractionConnection();
    virtual ConnectivityType Type() = 0;
public:
    PropertyValue *delay;
};

class AllToAllConnection: public AbstractionConnection
{
public:
    ConnectivityType Type(){return ALL_TO_ALL_CONNECTVITY_TYPE;}
};

class OneToOneConnection: public AbstractionConnection
{
public:
    ConnectivityType Type(){return ONE_TO_ONE_CONNECTVITY_TYPE;}
};

class ConnectionInstance
{
public:
    ConnectionInstance(){}
public:
    uint index;
    int src_neuron;
    int dst_neuron;
    double delay;
};

class ConnectionList: public AbstractionConnection
{
public:
    ConnectionList(){connection_count = 0;}
    ~ConnectionList();
    ConnectivityType Type(){return LIST_CONNECTVITY_TYPE;}
public:
    QMap<int, ConnectionInstance*> connectionIndices; //<index, connection instance>
    QHash <uint, QHash<uint, ConnectionInstance*> > connectionMatrix; //src, dst -> connection index
    uint connection_count;
};

class FixedProbabilityConnection: public AbstractionConnection
{
public:
    ConnectivityType Type(){return FIXED_PROBABILITY_CONNECTVITY_TYPE;}
public:
    double probability;
    int seed;
};

/* Experiment layer */

class LogOutput
{
public:
    LogOutput();
    ~LogOutput();

public:
    QString name;
    QString target;
    QString port;
    double start_time;
    double end_time;
    QSet <int> indices;
};

class Experiment
{
public:
    Experiment();
    ~Experiment();

public:
    double duration;
    double time_step;
    QHash<QString, LogOutput*> outputs; // key=target, multiple values per key!
    QString network_layer_url;
    //No input support yet!
};



#endif // MODELOBJECTS_H
