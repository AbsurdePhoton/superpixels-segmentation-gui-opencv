#include "tiffio.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <unistd.h>

bool MainWindow::SaveTiffMulti(std::string filename)
{
    char *file = &filename[0u];
    TIFF *out = TIFFOpen(file,"w") ;

    if (out) {
        int NPAGES = ui->listWidget_labels->count() +1; // number of page = nb labels + image

        //// image
        uint32 imagelength = image.rows;
        uint32 imagewidth = image.cols;
        uint8 * buf;
        uint32 row, col;
        uint16 nsamples = 3;

        TIFFSetField(out, TIFFTAG_IMAGELENGTH, imagelength); // image length
        TIFFSetField(out, TIFFTAG_IMAGEWIDTH, imagewidth); // image width
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG); // organized by RGB instead of channel
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, nsamples); // number of channels per pixel
        TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW); // compression
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8) ; // size of the channels
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, imagewidth*nsamples)); // strip size of the file to be size of one row of pixels
        TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // set the origin of the image
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB); // RGB image
        TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, "background"); // label name
        TIFFSetField(out, TIFFTAG_SOFTWARE, "Superpixels Segmentation GUI with openCV by AbsurdePhoton www.absurdephoton.fr"); // software used

        /* We are writing single page of the multipage file */
        TIFFSetField(out, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE); // new page
        TIFFSetField(out, TIFFTAG_PAGENUMBER, 0, NPAGES); // page number

        buf = new uint8 [imagewidth*nsamples] ;

        for (row = 0; row < imagelength; row++) {
            for(col=0; col < imagewidth; col++) {
                //writing data - order = RGB
                Vec3b c = image.at<Vec3b>(row, col);
                buf[col*nsamples+0] = c[2];
                buf[col*nsamples+1] = c[1];
                buf[col*nsamples+2] = c[0];
            }
            if (TIFFWriteScanline(out, buf, row) != 1 ) {
                return false;
            }
        }
        TIFFWriteDirectory(out);
        _TIFFfree(buf);

        //// Layers
        int page;
        for (page = 0; page < (NPAGES - 1); page++){
            QListWidgetItem *item = ui->listWidget_labels->item(page);
            QColor c = item->background().color();
            int nLabel = item->data(Qt::UserRole).toInt();
            std::string name = item->text().toUtf8().constData();

            uint32 imagelength = image.rows;
            uint32 imagewidth = image.cols;
            uint8 * buf;
            uint32 row, col;
            uint16 nsamples = 4;

            TIFFSetField(out, TIFFTAG_IMAGELENGTH, imagelength); // image length
            TIFFSetField(out, TIFFTAG_IMAGEWIDTH, imagewidth); // image width
            TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG); // organized by RGB instead of channel
            TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, nsamples); // number of channels per pixel
            TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW); // compression
            TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8) ; // size of the channels
            TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, imagewidth*nsamples)); // strip size of the file to be size of one row of pixels
            TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // set the origin of the image
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB); // RGB image
            TIFFSetField(out, TIFFTAG_EXTRASAMPLES, EXTRASAMPLE_ASSOCALPHA); // extra byte is alpha value
            TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, name); // label name
            TIFFSetField(out, TIFFTAG_PAGENAME, name); // label name
            TIFFSetField(out, TIFFTAG_SOFTWARE, "Superpixels Segmentation GUI with openCV by AbsurdePhoton www.absurdephoton.fr"); // software used

            /* We are writing single page of the multipage file */
            TIFFSetField(out, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE); // new page
            TIFFSetField(out, TIFFTAG_PAGENUMBER, page + 1, NPAGES); // page number

            buf = new uint8 [imagewidth*nsamples] ;

            for (row = 0; row < imagelength; row++) {
                for(col=0; col < imagewidth; col++) {
                    //writing data - order = RGB
                    int currentLabel = labels_mask.at<int>(row,col);
                    if (currentLabel == nLabel) {
                        buf[col*nsamples+0] = c.red();
                        buf[col*nsamples+1] = c.green();
                        buf[col*nsamples+2] = c.blue();
                        buf[col*nsamples+3] = 255;
                    }
                    else {
                        buf[col*nsamples+0] = 0;
                        buf[col*nsamples+1] = 0;
                        buf[col*nsamples+2] = 0;
                        buf[col*nsamples+3] = 0;
                    }
                }
                if (TIFFWriteScanline(out, buf, row) != 1 ) {
                    return false;
                }
            }
            TIFFWriteDirectory(out);
            _TIFFfree(buf);
        }
        TIFFClose(out);
    }
    return true;
} 
