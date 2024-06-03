#pragma once
#ifndef INTERRUPT_H
#define INTERRUPT_H

// Handler init
void sig_handler_init(void);
// Interrupt handler
void interrupt_handler(int signal);

#endif