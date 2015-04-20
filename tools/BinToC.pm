package BinToC;

sub binary_to_c_array($$$$$) {
    my ($input_file_path,
        $output_src_fh,
        $array_var_name,
        $end_ptr_var_name,
        $size_var_name) = @_;

    open my $input_file_handle, '<:bytes:raw', $input_file_path
      or die "Could not open input file $input_file_path!\n";
    print $output_src_fh "#include <stddef.h>\n";
    print $output_src_fh "#include <stdint.h>\n";
    print $output_src_fh "const uint8_t $array_var_name\[\] = { ";
    my $input_file_size = 0;
    while (defined (my $input_file_char = getc $input_file_handle)) {
      print $output_src_fh (sprintf "0x%x", ord $input_file_char);
      $input_file_size++;
      print $output_src_fh ", ";
    }
    close $input_file_handle;
    print $output_src_fh " 0x0 };\n";

    print $output_src_fh "const uint8_t * const $end_ptr_var_name\n";
    print $output_src_fh "    = $array_var_name + $input_file_size;\n";
    print $output_src_fh "const size_t $size_var_name\n";
    print $output_src_fh "    = $input_file_size;\n";
    print $output_src_fh "\n";
}

1;
