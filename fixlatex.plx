#!perl -w

my $doxy = "LuaService.doxyfile";
open IN, "<$doxy" or die "Can't open $doxy: $!\n";
open OUT, ">tmp.doxyfile" or die "Can't create tmp.doxyfile: $!\n";
while (<IN>) {
    s/^\s*(GENERATE_HTML\s*[=])\s*\S+\s*$/$1 NO/;
    s/^\s*(GENERATE_LATEX\s*[=])\s*\S+\s*$/$1 YES/;
    s/^\s*(GENERATE_RTF\s*[=])\s*\S+\s*$/$1 NO/;
    s/^\s*(GENERATE_MAN\s*[=])\s*\S+\s*$/$1 NO/;
    s/^\s*(GENERATE_XML\s*[=])\s*\S+\s*$/$1 NO/;
    s/^\s*(GENERATE_AUTOGEN_DEF\s*[=])\s*\S+\s*$/$1 NO/;
    s/^\s*(GENERATE_PERLMOD\s*[=])\s*\S+\s*$/$1 NO/;
    print OUT $_;
}
close IN;
close OUT;

system "doxygen tmp.doxyfile" and die "Couldn't run doxygen: $!\n";

my @files = glob "doc/latex/*.tex";
my $files = 0;
my $bf = 0;
my $verbatim = 0;
my $doxyref = 0;
my $includegraphics = 0;

foreach my $file (@files) {
    print $file, "\n";
    my $bak = "$file.bak";
    unlink $bak;
    rename($file, $bak) or die "Can't rename $file: $!\n";
    open IN, "<$bak" or die "Can't open $bak: $!\n";
    open OUT, ">$file" or die "Can't write $file: $!\n";
    while (<IN>) {
	# "\\bf{" --> "{\\bf " or "\\textbf{"
	#s/\\bf\{/{\\bf /g;
	$bf += s/\\bf\{/\\textbf{/g;

	# bugzilla implied this was also an issue, but apparently
	# not in my setup....
	# " \\doxyref{" --> " \\doxyref{}{"
	# $doxyref += s/\\doxyref\{/\\doxyref{}{/g;

	# doxygen 1.5.4 on windows in a folder with spaces in the path
	# can use unprotected file names in references to some loaded
	# images. Seen specifically with images from mscgen.
	$includegraphics += s/\\includegraphics\{([^}]+)\}/\\includegraphics{"$1"}/g;

	# \end{verbatim} should end a line
	$verbatim += s/\\end\{verbatim\}/\\end{verbatim}\n/g;
	print OUT $_;
    }
    ++$files;
    close OUT;
    close IN;
}


print "Changed $bf \\bf\n",
      "        $verbatim \\end{verbatim}\n",
#      "        $doxyref \\doxyref\n",
      "        $includegraphics \\includegraphics{\"...\"}\n",
      "    in $files files\n";

my $file = "doc/latex/Makefile";
print "Writing a better $file\n";
my $bak = "$file.bak";
unlink $bak;
rename($file, $bak) or die "Can't rename $file: $!\n";
open OUT, ">$file" or die "Can't write $file: $!\n";
print OUT <<END;
all: clean refman.pdf
pdf: refman.pdf

refman.pdf: refman.tex
	-pdflatex refman.tex
	-makeindex refman.idx
	-pdflatex refman.tex
	-pdflatex refman.tex
	pdflatex refman.tex
	copy refman.pdf ..\\LuaService.pdf

clean:
	-rm -f *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out refman.pdf
END
close OUT;

system "cd doc\\latex & make" and die "can't build LuaService.pdf";

