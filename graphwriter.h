#ifndef GRAPHWRITER_H
#define GRAPHWRITER_H

#include <QTextStream>
#include "writer.h"
#include "infoparser.h"

class GraphWriter : public SpineMLWriter
{
public:
    GraphWriter(QString output_filename, InfoParser *info);

    void writeDocumentStart();
    void writeDocuemntEnd();

    void writePopulation(Population *sub_population, Population *population = NULL);

private:
    QTextStream out;
    InfoParser *info;
};

#endif // GRAPHWRITER_H
