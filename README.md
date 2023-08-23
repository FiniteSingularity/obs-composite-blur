# OBS Composite Blur Plugin

## Introduction
OBS Composite Blur Plugin aims to be a comprehensive blur plugin that provides blur algorithms and types for all levels of quality and computational need.  The "composite" in the name is for providing proper compositing of blurring sources with alpha masks.  Since OBS sources have no context of the image underneath them, Composite Blur Plugin allows the user to provide a background source that is properly blurred into the resulting feathered edges.

## Blur Algorithms
OBS Composite Blur provides several different algorithms to blur your sources. The blur algorithms are written with performance in mind, using techniques like linear sampling and GPU texture interpolation to stretch what your GPU can do.

### Gaussian
A high quality blur algorithm that uses a gaussian kernel to sample/blur. Gaussian sampling results in an aestetically pleasing blur, but becomes computationally intensive at higher blur radius. This plugin supports fractional pixels for Gaussian blur, which allows for smooth animation when using plugins like Move Transition. Additionally, all platforms are supported, however due to how OBS handles OpenGL (MacOS and Linux) vs Direct3D (Windows), the OpenGL implementation is slightly less efficient. The Gaussian Blur Algorithm supports [Area](#area), [Directional](#directional), [Zoom](#zoom), and [Motion](#motion) blur effects.

### Box
Box blur works similar to Gaussian, but uses an equally weighted sample of surrounding pixels. The upside is a more efficient blurring algorithm, at the expense of some quality. With one pass, box blur can cause some blocky artifacts in some cases. This can be mitigated by increasing the number of passes- a 2 pass box blur has nearly the same quality as Gaussian blur. This plugin allows the user to specify up to 5 passes. Similar to Gaussian, this implementation of box blur allows for fractional pixels for smooth animation. The Box Blur Algorithm supports [Area](#area), [Directional](#directional), [Zoom](#zoom), and [Tilt-Shift](#tilt-shift) blur effects.

### Pixelate
Pixelate divides the souce into larger pixels, effectively downsampling the image, and giving it a bitmap like appearance.  This plugin allows the user to specify the pixel size and shape.  Supported shapes are Square, Hexagon, Triangle, and Circle. As with the other algorithms, fractional pixel sizes (blur radius) is supported.  The Pixelate Algorithm only supports the [Area](#area) blur effect.

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