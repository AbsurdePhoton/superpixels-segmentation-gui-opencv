# superpixels-segmentation-gui-opencv
## Superpixels segmentation algorithms with QT and OpenCV, with a nice GUI to colorize the cells
### v0 - 2018-08-20 - first version

![Screenshot](screenshot.jpg)
<br/>

## HISTORY

* v0: first try
<br/>
<br/>

## LICENSE

The present code is under GPL v3 license, that means you can do almost whatever you want
with it!
I used bits of code from several sources, mainly from the openCV examples
<br/>
<br/>

## WHY?

I didn't find any simple tool (understand: GUI) to produce depth maps from my stereo pictures.
One solution to semi-automatically produce depth maps is image segmentation: coloring zones with superpixel cells is easy at first. In photoshop, it is child play to apply gray gradients to colored areas...
I'm not an ace of C++ and QT, in fact I only started using them some month ago. So, if you don't find my code pretty never mind, because it WORKS, and that's all I'm asking of it :)
<br/>
<br/>

## WITH WHAT?

Developed using:
* Linux: Ubuntu	16.04
* QT Creator 3.5.1
* QT 5.5.1
* openCV 3.4.1

This software should also work under Microsoft Windows: if you tried it successfully please contact me.
<br/>
<br/>

## HOW?

* Load an image first, nothing is possible without it
* Select the algorithm, tune its parameters
* Select the pre-processing operations on the image: gaussian blur, contours, etc
* Push the COMPUTE button, wait a little
* The result is displayed. You have 3 layers that you can activate / deactivate / blend :
** The processed image (gaussian blur, contours, etc)
** The (for the moment) empty colored mask
** The cells grid
* Now you can colorize !
** left mouse button to colorize cells with the the current color (found on the right, change it with the big colored buttons)
** right mouse button to unset a cell
** <ALT> + click to fill a closed area
* Change the view:
** you can choose the transparency of the image and mask with the sliders on the right
** you can hide / view the 3 layers: image, mask and grid
** use the zoom controls to zoom in / out - you can also use the mouse wheel over the viewport
** hold the middle mouse button on the viewport to move the view position - you can also use the scrollbars
** click on the thumbnail to change the view position
* Load and save:
** you can save and retrieve the segmentation parameters (XML file)
** you can save the current session : no need to recompute when you retrieve it!
** what is saved in the session: processed image (PBG), mask(PNG), grid (PNG) and cells / labels (XML openCV format) 
<br/>
<br/>

Enjoy!
<br/>

## Enjoy!

### AbsurdePhoton
My photographer website ''Photongénique'': www.absurdephoton.fr
