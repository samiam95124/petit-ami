# graphics stacking plan

With the advent of a frame buffering interface, a solid plan is needed for the
way graphics implementations are stacked up.

## The goal: embedded implementations

Access to a frame buffer level is primarily aimed at embedded implementations.
Although interfacing at the frame buffer level would seem to pin it to Linux,
the frame buffer level is a universal interface. It describes a graphics frame
buffer resident in memory, which the CPY can then develop pixel level routines
to write on, or even read as required. Thus it leads to two distinct levels of
embedded use:

1. Within embedded linux.
2. As a complete stand-alone embedded layer.

Since Linux is already well covered with graphics implementations (X11, 
Wayland), the advantage of use in an embedded Linux comes down to reduced
memory footprint and reduced hardware requirements.

## Overrides: the means to stack interfaces

The means to stack a complex interface like a full graphics system was 
implemented using overrides. For each overridable function call, a vector is
kept to that function and a overrider routine exists to place a new vector to
that function as well as keep the previous contents of the vector. That last is
important because it allows the overrider to implement what is commonly refered
to as "hooking" the vector. The original function points to the new, replacement
function, but that function also has the ability to call the old function. This
can occur for any number of interface layers, creating a stacking system.

This system has pros and cons:

Pro:

- Layers can stack without regard to what the previous stacking layer was.
- The stacking sequence is determined by the link order, and not compile order.

Cons:

- Adds overhead.
- Often a strict order of linking and/or initialization must be observed to get
  the correct override order.

  The overhead of this method is:

  Runtime: the function call must go through a layer of indirection. Usually
  this is a single instruction.
  Code size: A vector and a routine to perform the override must be added.

  Of course many languages provide overrides as a language feature. This does
  not remove the cost, but it does make implementation easier.

  For the purposes of this document, the cost is considered well worth it.

  ## Stacking order

  The logical stacking order of the graphics system is:

  framebuffer<-graphics<-windowing<-widgets and dialogs

  The basis for this is that each layer can be built solely using the funtional
  API exposed by the module below it.

  ## Acceleration

  Many graphics cards have hardware accelleration features, in torms of figure
  drawing, block pixel moves, 3d triangles, frame buffer flipping and others.
  
  These accelerators are not necesarily better. Sometimes the CPU can beat them
  for speed on small shapes where the overhead to couple the hardware exceeds
  the draw time advantage. Its also possible that the offload from the CPU is
  valuable even if there is no draw time advantage.

  The accelerators fit into the overload system, replacing the CPU draw 
  routines. This means they go into the overload system first, before the upper
  layer routines that use them.

