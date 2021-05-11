# Hammer

A compilation of bad practices and anti-patterns.

## Terrain generation
Terrain generation is currently (2021-05-11) composed of three steps, each of which is an amalgamation of research papers, held together with crap code (my contribution):

#### Lithosphere simulation
> Lauri Viitanen, **Physically Based Terrain Generation: Procedural Heightmap Generation Using Plate Tectonics**, 2012. http://urn.fi/URN:NBN:fi:amk-201204023993.

I have been playing around with different methods of modeling tectonic plate interaction, but somehow the idea to separate each plate into its own layer never seemed feasible to me until I read this paper. I always attempted to deterministically project the position of plates and calculate boundaries in one step, but this paper proves an iterative approach is feasible even on very memory constrained devices, and breaking the domain into plates more than once produces much more interesting topography.

I did not use or read [Lauri's "reference" implementation](https://sourceforge.net/projects/platec/) because the GPL scares me, and I knew I would be rewriting it in C anyway. My results are not even close if you run the two side-by-side. I definitely, intentionally and unintentionally, cut corners, but my goal was never physical realism.

#### Climate system
This is a cobbled together, late-night hack that definitely needs improvement. It is basically a thermodynamic fluid simulation with moisture advection, based upon the shallow-water and sediment transport model presented by Balázs Jákó and Balázs Tóth in a paper I've studied extensively in the past:
> Balázs Jákó, Balázs Tóth **Fast Hydraulic and Thermal Erosion on the GPU**, 2011.

#### Stream tree generation and tectonic uplift
> Guillaume Cordonnier, Jean Braun, Marie-Paule Cani, Bedrich Benes, Eric Galin, et al. **Large Scale Terrain Generation from Tectonic Uplift and Fluvial Erosion.** Computer Graphics Forum, Wiley, 2016, Proc. EUROGRAPHICS 2016, 35 (2), pp.165-175. 10.1111/cgf.12820. hal-01262376

I freaking love this paper. Go read it. Go read every HAL terrain generation paper because they are all expertly composed and explained. This is the least cobbled-together mess, and that's entirely due to the quality of this paper. My implementation differs very little from the described implementation.

## Build

#### GitHub Dependencies
Clone this repository with --recurse, build and install each in external/ using CMake.
| Project | |
| ------- | - |
| [raintherrien/deadlock](https://github.com/raintherrien/deadlock) | Async tasks |
| [raintherrien/delaunator-c](https://github.com/raintherrien/delaunator-c) | C Port of [Delaunator](https://github.com/mapbox/delaunator) |
| [recp/cglm](https://github.com/recp/cglm) | Graphics math library |

#### Other Library Dependencies
| Library |
| ------- |
| [SDL2 and SDL2_image](https://www.libsdl.org/) |
| [GLEW](http://glew.sourceforge.net/) |
| (OPTIONAL) [libpng](http://www.libpng.org/pub/png/libpng.html) |
