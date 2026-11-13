#ifndef LOGGER_H
#define LOGGER_H

#define LOG_FILE      "campus.log"
#define MAX_PRODUCERS 8
#define MAX_EVENTS    50

int run_logger_demo(int producers, int events_each);

#endif
