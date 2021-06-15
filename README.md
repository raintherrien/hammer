# hammer

A compilation of bad practices and anti-patterns. Don't judge me for this code. Especially if I sent you my resume *wink wink*. There are two topics I want to explore with hammer:
#### Feasibility of fine grained task scheduling using [deadlock](https://github.com/raintherrien/deadlock)
Stackful coroutines have already seen widespread adoption in the form of ucontext, [Windows Fibers](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiber), [Boost.fiber](https://github.com/boostorg/fiber), Goroutines, and C#'s Task Parallel Library to name a few.

There are plenty of C libraries that dip into the undefined nether realms of `asm` to bring us stackful coroutines but let's be honest, no one wants to deal with that. ARM is probably the future and I don't really want to learn another ISA.

With C++20 we now have stackless coroutine support in the standard library. There are plenty of event driven frameworks too. [Some languages](https://ziglang.org/) even incorporate one into the standard library. That's basically what deadlock is but with **transparent memory allocation** and **dead simple task graphs** with no concurrency controls required.

#### Modern procedural programming; Not functional, not Object Oriented
The heap is overrated. What ever happened to .bss?
Let's talk about the anti-global sentiment.
Wasn't file scope really the OG object?
Seriously though go read some of the code [id Software](https://github.com/id-Software) open sourced. It's fantastic.

I find it interesting that multithreaded code and global state are often presented irreconcilably. Of course it is true that blindly multithreading code that mutates a global state is undefined behavior, and best it's not easy to get **correct**, what I hope to learn from writing hammer this way is how to write multithreaded code **well**.
I want to stop asking the questions: *Does this need a callback? Why do you need an atomic shared_ptr? Who even owns this data???*

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

#### Hexagon nonsense
I'm still digesting the full implications of choosing a hexagonal coordinate space, but everything so far (code, musings) comes from Amit Patel's excellent article on the subject.
> Hexagonal Grids from Red Blob Games
> https://www.redblobgames.com/grids/hexagons/
> Copyright (c) 2021 Amit Patel
> Distributed under the MIT license:
> https://opensource.org/licenses/mit-license.php

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
