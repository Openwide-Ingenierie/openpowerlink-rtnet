openPOWERLINK Demo applications {#page_demos}
===============================

[TOC]

# Introduction
The openPOWERLINK stack distribution contains a set of demo applications which
show how the openPOWERLINK stack is used by an application.

## Building demo applications
To build a demo application the necessary openPOWERLINK libraries must be
available.

## Available demo applications

# Console MN demo {#sect_demos_mnconsole}

This demo implements a POWERLINK MN with CFM. It is realized as console
application and is intended for machines where no graphical user interface is
available.

The demo can be found in: `apps/demo_mn_console`

# Console CN demo {#sect_demos_cnconsole}

This demo implements a POWERLINK CN digital I/O node according to the CiA401
profile. It is implemented as console application.

It is located in: `apps/demo_cn_console`

# QT MN demo {#sect_demos_mnqt}

The QT demo application implements a POWERLINK managing node (MN) using the
configuration manager (CFM) to initialize the controlled nodes. It uses a
network configuration created with the openCONFIGURATOR tool.
  
The demo can be found in: `apps/demo_mn_qt`

# Embedded CN demo {#sect_demos_cnembedded}

This demo implements a POWERLINK CN digital I/O node according to the CiA401
profile. It is implemented as an embedded application.

It is located in: `apps/demo_cn_embedded`

# Embedded MN demo {#sect_demos_mnembedded}

This demo implements a POWERLINK MN with CFM. It is implemented as an embedded
application.

It is located in: `apps/demo_mn_embedded`
