#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <qendian.h>
#include <string.h>

#ifndef __FILENAME__
    #if __cplusplus > 201103L
        namespace Logger {
            constexpr auto * extractFileName(const char * const path)
            {
                const auto * startPosition = path;
                for (const auto* currentCharacter = path; *currentCharacter != '\0'; ++currentCharacter) {
                    if (*currentCharacter == '\\' || *currentCharacter == '/') {
                        startPosition = currentCharacter;
                    }
                }
                if (startPosition != path) {
                    ++startPosition;
                }
                return startPosition;
            }
        }
        #define __FILENAME__ Logger::extractFileName(__FILE__)
    #else
        #define __FILENAME__ (strrchr(__FILE__, '/') != Q_NULLPTR ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') != Q_NULLPTR ? strrchr(__FILE__, '\\') + 1 : __FILE__)
    #endif
#endif

#ifndef LOG_FILE_NAME_WIDTH
#define LOG_FILE_NAME_WIDTH 24
#endif
#ifndef LOG_CATEGORY_WIDTH
#define LOG_CATEGORY_WIDTH  15
#endif
#ifndef toPrintable
#define printString(a) a.toUtf8().constData()
#define toPrintable(a) printString(QString(a))
#endif
#define DATETIME_LABEL QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy.MM.dd hh:mm:ss.zzz"))
#define _fline_ QStringLiteral("{%1;%2} ").arg(QString(QLatin1String(__FILENAME__)).leftJustified(LOG_FILE_NAME_WIDTH,' ',true)).arg(QString::number(__LINE__).rightJustified(4, ' '))
#define CURRENT_THREAD_INFO QStringLiteral("[%1] ").arg(QStringLiteral("thr: %1").arg(qToBigEndian<quint64>(reinterpret_cast<quint64>(QThread::currentThread()))).leftJustified(9, ' ', true))

#define FATAL_LABEL  " (Ftl) "
#define EXCEPT_LABEL " (Exc) "
#define IMP_LABEL    " (Imp) "
#define ERROR_LABEL  " (Err) "
#define WARNG_LABEL  " (Wrn) "
#define NOTE_LABEL   " (Ntc) "
#define INFO_LABEL   " (Inf) "
#define DEBUG_LABEL  " (Dbg) "
#define TRACE_LABEL  " (Trc) "
#define NOTSET_LABEL "       "

extern int maxloglevel;

#define _begin_output_log_by_level(level) do{if(maxloglevel >= level){
#define _begin_output_log_ do{{
#define _end_output_log_ "";}}while(0)
#define end_log _end_output_log_

#define log_fatal _begin_output_log_ QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(FATAL_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).fatal()
#define log_except _begin_output_log_by_level(1) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(EXCEPT_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).critical()
#define log_error _begin_output_log_by_level(1) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(ERROR_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).critical()
#define log_important _begin_output_log_ QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(IMP_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).info()
#define log_warning _begin_output_log_by_level(2) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(WARNG_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).warning()
#define log_note _begin_output_log_by_level(3) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(NOTE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).info()
#define log_info _begin_output_log_by_level(4) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(INFO_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).info()
#define log_debug _begin_output_log_by_level(5) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(DEBUG_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace _begin_output_log_by_level(6) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_1 _begin_output_log_by_level(7) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_2 _begin_output_log_by_level(8) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_3 _begin_output_log_by_level(9) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_4 _begin_output_log_by_level(10) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_5 _begin_output_log_by_level(11) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()
#define log_trace_6 _begin_output_log_by_level(12) QMessageLogger("", 0, "", printString(DATETIME_LABEL.append(TRACE_LABEL).append(CURRENT_THREAD_INFO).append(_fline_))).debug()


#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
#define printByteArray(ba) "["<<ba.length()<<"]"<<ba.toHex().constData()
#else
#define printByteArray(ba) "["<<ba.length()<<"]"<<ba.toHex(' ').constData()
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
#define printByteArrayPartly(ba, count) "["<<ba.length()<<"]"<<QByteArray::fromRawData(ba.constData(), qMin(ba.length(),count)).toHex().constData()<<(ba.length()<count?"":"...")
#else
#define printByteArrayPartly(ba, count) "["<<ba.length()<<"]"<<QByteArray::fromRawData(ba.constData(), qMin(ba.length(),count)).toHex(' ').constData()<<(ba.length()<count?"":"...")
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
#define printRawByteArray(data, len) "["<<len<<"]"<<QByteArray::fromRawData(data, len).toHex().constData()
#else
#define printRawByteArray(data, len) "["<<len<<"]"<<QByteArray::fromRawData(data, len).toHex(' ').constData()
#endif

#define printCStr(ba) ba.constData()


#endif // LOGGER_H
