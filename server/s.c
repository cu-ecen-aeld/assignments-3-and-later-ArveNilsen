size_t seg_len = (nl - pos) + 1;

/* Avoid overflow */
if (!packet_fits(sb->len, seg_len, MAX_PACKET)) {
	sb->len = 0;
	pos += seg_len;
	remaining = (size_t)(end - pos);
	continue;
}

size_t packet_len = 0;
if (assemble_packet(scratch, 
			MAX_PACKET, 
			sb->str, 
			sb->len,
			pos, 
			seg_len, 
			&packet_len) == -1) {
	return EXIT_ERROR;
}

if (write_all(write_fd, scratch, packet_len) 
		== -1) {
	return EXIT_ERROR;
}

if (send_file_to_client(fd, 
			AESD_DATA_PATH) == -1) {
	return EXIT_ERROR;
}

sb->len = 0;
pos += seg_len;
remaining = (size_t)(end - pos);
