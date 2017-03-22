#ifndef UPNQT_GLOBAL_H
#define UPNQT_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(UPNQT_LIBRARY)
#  define UPNQTSHARED_EXPORT Q_DECL_EXPORT
#else
#  define UPNQTSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // UPNQT_GLOBAL_H
