#include "writer.h"

#include <iostream>

SpineMLWriter::SpineMLWriter(QString output_filename)
{
    output_file = new QFile(output_filename.toLocal8Bit().data());
    if (!output_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Error opening output file: " << output_filename.toLocal8Bit().data()  << std::endl;
        exit(0);
    }
}

void SpineMLWriter::close()
{
    output_file->close();
    delete output_file;
}
