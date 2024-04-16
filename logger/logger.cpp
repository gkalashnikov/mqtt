#include "logger.h"
#include <QCalendar>

// fix ASSERT: "byId[size_t(id)] == nullptr" in file time\qcalendar.cpp
static const QCalendar cal;
