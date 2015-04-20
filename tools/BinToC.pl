#!/usr/bin/perl -w

# This script does the same what GNU Binutil's "ld -b binary" does, but
# in a more portable way. It creates a small assembler program with an
# ".incbin" directive to embed the file and assembles it.

use strict;
use lib './tools';
use File::Basename;
use BinToC;

my $input_file_path;
my $output_src_file_path;

($input_file_path, $output_src_file_path) = @ARGV;

my($input_file_name, $input_file_dir, $input_file_ext)
  = fileparse $input_file_path;

my $array_name = "${input_file_name}${input_file_ext}";
$array_name =~ s,\.,_,g;

open my $output_src_fh, '>', $output_src_file_path
  or die "Could not source open output file $output_src_file_path!\n";

BinToC::binary_to_c_array($input_file_path,
                          $output_src_fh,
                          $array_name,
                          $array_name."_end",
                          $array_name."_size");
close $output_src_fh;
