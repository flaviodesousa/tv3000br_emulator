An old (MSDOS anyone?) terminal emulator.

Between 1986 and 1989 I programmed on a language called MUMPS on minicomputers. Those programs showed UI on serial terminals with special features activated by sending special character sequences like "<ESC>=!!" which allowed positioning the cursor, changing screen attributes (as blinking or underlined), protecting text from being changed, set scroll areas, etc.

Someday I had to port some applications to a PC version of the language. On this PC version there would be no serial terminals, but only the PC Video (CGA) to interact with. Changing all the applications to send codes working on the PC (using MSDOS ANSI.SYS driver)should be a huge task, that would consume weeks, if not months of work. Not forgetting ANSI.SYS did not have equivalent features (e.g. scroll zones)

Then I came with this idea: instead of converting the application, why not make the PC to understand the serial terminal codes? Instead of a driver I wrote it as a TSR which disabled the Video BIOS (INT 0x10) and took over while the MUMPS environment was running. This approached took only a couple of weeks and required NO changes on the application! #Win