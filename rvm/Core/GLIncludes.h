//
//  GLIncludes.h
//  rvm
//

#ifndef GLIncludes_h
#define GLIncludes_h

#if WINDOWS
# include <Windows.h>
# include <GL/glew.h>
# pragma comment(lib, "glew32s.lib")
#elif LINUX
# include <gl/GL.h>
# include <gl/glext.h>
#elif __VITA__
# include <vitaGL.h>
#else
# include <OpenGL/gl.h>
#endif

#endif // GLIncludes_h
