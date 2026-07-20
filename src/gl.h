#ifndef NEONFALL_GL_H
#define NEONFALL_GL_H

#ifdef VITA
  #include <vitaGL.h>
#elif defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif

#endif
