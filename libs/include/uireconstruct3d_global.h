#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(UIRECONSTRUCT3D_LIB)
#  define UIRECONSTRUCT3D_EXPORT Q_DECL_EXPORT
# else
#  define UIRECONSTRUCT3D_EXPORT Q_DECL_IMPORT
# endif
#else
# define UIRECONSTRUCT3D_EXPORT
#endif
