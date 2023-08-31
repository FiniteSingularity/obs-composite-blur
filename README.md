# OBS Composite Blur Plugin

## Introduction
OBS Composite Blur Plugin aims to be a comprehensive blur plugin that provides blur algorithms and types for all levels of quality and computational need.  The "composite" in the name is for providing proper compositing of blurring sources with alpha masks.  Since OBS sources have no context of the image underneath them, Composite Blur Plugin allows the user to provide a background source that is properly blurred into the resulting feathered edges.

## Blur Algorithms
OBS Composite Blur provides several different algorithms to blur your sources. The blur algorithms are written with performance in mind, using techniques like linear sampling and GPU texture interpolation to stretch what your GPU can do.

### Gaussian
A high quality blur algorithm that uses a gaussian kernel to sample/blur. Gaussian sampling results in an aestetically pleasing blur, but becomes computationally intensive at higher blur radius. This plugin supports fractional pixels for Gaussian blur, which allows for smooth animation when using plugins like Move Transition. Additionally, all platforms are supported, however due to how OBS handles OpenGL (MacOS and Linux) vs Direct3D (Windows), the OpenGL implementation is slightly less efficient. The Gaussian Blur Algorithm supports [Area](#area), [Directional](#directional), [Zoom](#zoom), and [Motion](#motion) blur effects.

### Box
Box blur works similar to Gaussian, but uses an equally weighted sample of surrounding pixels. The upside is a more efficient blurring algorithm, at the expense of some quality. With one pass, box blur can cause some blocky artifacts in some cases. This can be mitigated by increasing the number of passes- a 2 pass box blur has nearly the same quality as Gaussian blur. This plugin allows the user to specify up to 5 passes. Similar to Gaussian, this implementation of box blur allows for fractional pixels for smooth animation. The Box Blur Algorithm supports [Area](#area), [Directional](#directional), [Zoom](#zoom), and [Tilt-Shift](#tilt-shift) blur effects.

### Dual Kawase (Dual Filter)
Dual Kawase is a blurring algorithm that uses down and upsampling in order to blur the image.  It has a high quality blur with little artifacting and is computationally efficient especially at larger blur values.  The naieve implementation of Dual Kawase however has very large jumps in blur- essentially doubling the blur at each step.  This implementation sacrafices a small amount of efficiency, but allows intermediate blur values by using linear interpolation during the final downsamping step.  This gives a continuous change in blur values.  The most common use for Dual Kawase is when you need a very high blur radius, but with linear interpolation it can also be used as a general blur.  However due to how the algorithm works, only [Area](#area) blur is available.

### Pixelate
Pixelate divides the souce into larger pixels, effectively downsampling the image, and giving it a bitmap like appearance.  This plugin allows the user to specify the pixel size and shape.  Supported shapes are Square, Hexagon, Triangle, and Circle. As with the other algorithms, fractional pixel sizes (blur radius) are supported.  The Pixelate Algorithm only supports the [Area](#area) blur effect.

## Blur Effects
OBS Composite Blur provides several different blur effects or types, all giving a different feel to the resulting image.

### Area
Area blur is the typical 2D blur where pixels are blurred equally in all directions. Inputs are only the blur radius.

### Directional
Directional blur is a blur applied along a single axis, but is blurred in both the positive and negative direction.  Inputs are blur radius and the direction angle.

### Motion
Motion blur is similar to [Directional Blur](#directional), however it is applied in
only the positive direction along the blur axis. This yields an image that simulates a camera capturing blur due to motion in a particular direction. Inputs are blur radius and direction angle.

### Zoom
Zoom blur is applied away from a center zoom point, and increases the further from the center point the pixel being blurred is. This yields an image that looks like the viewer is zooming into the center zoom point. Inputs are blur radius, and center zoom point location.

### Tilt-Shift
Tilt-Shift blur defines an in-focus plane, specified by a location in the frame, and a thickness.  All pixels outside of the in-focus plane have their blur value increased the further away from the plane they are. The resulting image gives a distorted sense of scale, making large objects look like mineature models. When applied to video scenes like a city street, the effect can be significant.  Inputs are blur radius, focus plane angle, focus plane location, and focus plane thickness.

## Effect Masking
OBS Composite Blur offers a variety of ways to mask where and how blur is applied to your source. For all mask options, the mask can also be inverted by checking the "Invert Mask" box.  The following options are available.

### Crop
Specify the percentage distance in from the top, bottom, left, and right edges of your source that you want masked. Additionally, the crop mask allows you to specify a corner radius for nice, smooth rounded corners.

### Source
Use another OBS source or scene as a mask for your blur.  Simply select the source or scene you want to use, and then specify if you want to use the source's alpha channel, grayscale value, luminosity, or a custom combination of the red, green, blue, and alpha channels to mask the blur effect. You can also multiply the resulting mask by a value.  The multiply value comes in handy if you have a translucent source, but want everything behind the translucent source to be fully blurred.

### Image
All of the same options as [Source](#source), but allows you to select an image file rather than a source.

### Rectangle
Is the same as the [Crop](#crop) option, but instead of specifying the edges, you specify the center of the rectangle, the rectangle width, and rectangle height. This is easier to use with plugins like Move Transition if you want to animate the movement or size of the rectangular masked blur.

### Circle
Similar to the [Rectangle](#rectangle) option, but lets you specify the center of a circle and its radius. Some nice sweep effects can be made by using a very large circle, and moving it from off the source (less than 0 or greater than 100 for the center coordinates) over the source.
