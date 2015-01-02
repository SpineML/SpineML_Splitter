#ifndef SPINEMLWRITER_H
#define SPINEMLWRITER_H

#include <QFile>

#include "modelobjects.h"

typedef enum{
    WRITER_MODE_XML,
    WRITER_MODE_ALIAS,
    WRITER_MODE_GRAPH
}WriterMode;

class SpineMLWriter
{
public:
    SpineMLWriter(QString output_filename);

    virtual void writeDocumentStart() = 0;
    virtual void writeDocuemntEnd() = 0;

    virtual void writePopulation(Population *sub_population, Population *population = NULL) = 0;

    void close();

protected:
    QFile* output_file;
};

#endif // SPINEMLWRITER_H
