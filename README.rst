===
ccc
===

ccc - "compiler construction course" program.

References:

- Aho, Sethi, Ullman "Compilers: Principles, Techniques, and Tools"
- Muchnick "Advanced Compiler Design and Implementation"

The following instructions have been tested with Ubuntu.

Compiling
---------

.. code:: bash

   git clone -b experimental git://github.com/BDanAnd/ccc.git
   cd ccc
   mkdir build
   cd build
   cmake ..
   make

Usage
-----

Example:

.. code:: bash

   ./ccc < sourcecode.txt > output.txt -G

Run program with a flag "-u" or "-usage" to view the available options. Use flag "-h" or "-help" to view more information. Flag "-friendly" allows for a simple input file format. Only printer and analysis options give information about the source code. Therefore, the launch of the program only to optimize the code is not possible. If you specify any of optimization options, all selected printers and analyzes will be given information before and after optimization. All the selected optimization algorithms will be applied consistently and cyclically until the source code is no longer change.

