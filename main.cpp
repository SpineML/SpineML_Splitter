#include <iostream>
#include <QTextStream>
#include <QElapsedTimer>
#include <QDebug>

#include "splitter.h"

void printUsage(){
    std::cout << "Usage: SpineMLSplitter input_file output_file [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "   -no_parallel        Turns off multicore splitting optimisations" << std::endl;
    std::cout << "   -no_xml_formatting  Turns off xml autoformatting in default xml output (ignored when -alias is used)" << std::endl;
    std::cout << "   -alias              Writes split file to DAMSON alias file (ignores no_xml_formatting)" << std::endl;
    std::cout << "   -silent             Turns off console reporting of splitter and writer progress" << std::endl;
}

QString formatMillis(uint ms){
    uint sec_temp = (ms+500) / 1000;
    uint m = sec_temp / 60;
    uint s = sec_temp - (m * 60);
    uint millis = ms % 1000;
    QString format = QString::number(m).append(" mins, ").append(QString::number(s)).append(" secs, ").append(QString::number(millis)).append( " ms");
    return format;
}

int main(int argc, char** argv)
{
    SpineMLSplitter *splitter;
    bool parallel = true;
    bool formatting = true;
    bool silent = false;
    WriterMode mode = WRITER_MODE_XML;

    //check argument count
    if (argc < 3){
        printUsage();
        exit(0);
    }

    //handle arguments
    QString input_file = QString(argv[1]);
    QString output_file = QString(argv[2]);
    for (int i=3; i<argc; i++){
        QString arg = QString(argv[i]);
        if (arg == "-no_parallel")
            parallel = false;
        else if (arg == "-no_xml_formatting")
            formatting = false;
        else if (arg == "-silent")
            silent = true;
        else if (arg == "-alias")
            mode = WRITER_MODE_ALIAS;
        else if (arg == "-graph")
            mode = WRITER_MODE_GRAPH;
        else{
            std::cerr << "Unrecognised argument!" <<std::endl;
            printUsage();
            exit(0);
        }
    }

    splitter = new SpineMLSplitter(parallel, formatting, silent, mode);

    splitter->split(input_file, output_file);

    std::cout << "Completed Time: " << formatMillis(splitter->getTotalTime()).toLocal8Bit().data() << std::endl;
    std::cout << "Splitting Time: " << formatMillis(splitter->getSplitTime()).toLocal8Bit().data() << std::endl;

    std::cout << (splitter->getParser()->getParsedPopulationCount()) << " Parsed Populations" << std::endl;
    std::cout << (splitter->getParser()->getParsedProjectionCount()+splitter->getParser()->getParsedInputCount()) << " Parsed Projections/Inputs" << std::endl;
    std::cout << splitter->getSplitPopulationCount() << " Sub Populations" << std::endl;
    std::cout << splitter->getSplitProjectionCount()+splitter->getSplitInputCount() << " Sub Projections/Inputs " << std::endl;

    delete splitter;
}
