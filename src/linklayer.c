#include "linklayer.h"

//* ------------------------------------------------------------------

bool alarm_flag = false;
int alarm_counter = 0;

void alarm_handler(int signum);

int serial_open(char* serial_port);
int serial_config(linkLayer* ll);
void serial_close(int fd, struct termios* configs);

bool send_frame(linkLayer ll, sds packet, frame_type ft_expected);

uchar calculate_bcc2(sds data);
uchar validate_bcc2(sds data);

//* ------------------------------------------------------------------

int llopen(linkLayer* ll) {
    ll->fd = serial_open(ll->serialPort);
    if (ll->fd < 0) {
        ERROR("Error opening serial port %s\n", ll->serialPort);
        return -1;
    }

    if (serial_config(ll) < 0)
        return -1;

    if (ll->role == RECEIVER)
        return 0;

    sds set = sdsnewframe(ft_SET);
    bool success = send_frame(*ll, set, ft_UA);
    sdsfree(set);

    if (success)
        INFO("-----\nHandshake complete\n-----\n\n");
    else
        ERROR("Handshake failure - check connection\n");

    return success;
}

int llwrite(linkLayer ll, char* filepath) {
    int file = open(filepath, O_RDONLY);
    if (file == -1) {
        ERROR("Could not open '%s' file\n", filepath);
        return false;
    }

    int id = 0;
    uchar buf[ll.payload_size];

    bool success = false;

    while (true) {
        int nbytes = read(file, &buf, ll.payload_size);
        if (!nbytes)
            break;

        frame_type ft_expected = id ? ft_RR0 : ft_RR1;
        frame_type ft_format = id ? ft_INFO1 : ft_INFO0;

        //* Create the INFO frame header (without BCC2 and Last Flag)
        sds header = sdsnewframe(ft_format);
        sdsrange(header, 0, -2);

        //* Create data string from buf and calculate bcc2
        sds data = sdsnewlen(buf, nbytes);
        uchar bcc2 = calculate_bcc2(data);
        uchar tail1[] = {bcc2, '\0'};
        uchar tail2[] = {F, '\0'};

        //* Append bcc2 to data frame
        data = sdscat(data, (char*)tail1);
        //* Byte-stuff date and bcc2
        sds stuffed_data = byte_stuffing(data);
        sds data_formated = sdscatsds(header, stuffed_data);
        //* Append final flag
        data_formated = sdscat(data_formated, (char*)tail2);

        sds data_repr = sdscatrepr(sdsempty(), data, sdslen(data));
        INFO("Sending data frame: \n\t>>%s\n\t>>Lenght: %ld\n", data_repr, sdslen(data));
        LOG("Calculated BCC2: 0x%.02x = '%c'\n\n", (unsigned int)(bcc2 & 0xFF), bcc2);
        success = send_frame(ll, data_formated, ft_expected);
        if (!success)
            break;

        id = !id;

        sdsfree(data);
        sdsfree(data_repr);
        sdsfree(stuffed_data);
        sdsfree(data_formated);
    }

    close(file);
    return success - 1;
}

int llread(linkLayer ll, char* filename) {
    uchar rcved;

    frame_type current_frame = ft_ANY;
    frame_state current_state = fs_FLAG1;

    sds ua = sdsnewframe(ft_UA);
    sds rr0 = sdsnewframe(ft_RR0);
    sds rr1 = sdsnewframe(ft_RR1);
    sds disc = sdsnewframe(ft_DISC);

    sds info_buf = sdsempty();
    sds data = sdsempty();

    bool close = false;
    bool terminate_connection = false;

    int file = open(filename, O_WRONLY | O_CREAT, 0666);
    if (file < 0) {
        ERROR("Error opening file %s\n", filename);
        exit(1);
    }

    while (true) {
        if (close)
            break;

        int nbytes = read(ll.fd, &rcved, sizeof rcved);
        if (!nbytes)
            continue;

        current_state = frame_handler(current_state, &current_frame, rcved);

        if (current_frame == ft_INVALID) {
            break;
        }

        if (current_state == fs_INFO) {
            // LOG("Adding 0x%.02x = '%c' to buffer\n\n", (unsigned int)(rcved & 0xFF), rcved);
            char buf[] = {rcved, '\0'};
            info_buf = sdscat(info_buf, buf);
            // LOG("Current Buffer: %s\n\n", info_buf);
        }

        if (current_state == fs_BCC2_OK) {
            sdsupdatelen(info_buf);
            data = byte_destuffing(info_buf);
            //* Remove bcc1 fom the beginning of the buffer.
            sdsrange(data, 1, -1);
            current_state = frame_handler(current_state, &current_frame, validate_bcc2(data));
            //* Remove bcc2 from the end of the buffer.
            sdsrange(data, 0, -2);
            sds data_repr = sdscatrepr(sdsempty(), data, sdslen(data));
            INFO("Received data frame: \n\t>>%s\n\t>>Lenght: %ld chars\n", data_repr, sdslen(data));
            sdsfree(data_repr);
        }

        if (current_state == fs_VALID) {
            close = true;
            switch (current_frame) {
                case ft_SET:
                    write(ll.fd, ua, sdslen(ua));
                    break;

                case ft_UA:
                    INFO("Received Last Flag. Closing connection.\n");
                    terminate_connection = true;
                    break;

                case ft_DISC:
                    write(ll.fd, disc, sdslen(disc));
                    terminate_connection = true;
                    break;

                case ft_INFO0:
                    INFO("Writing data packet to file\n\n");
                    write(file, data, sdslen(data));
                    write(ll.fd, rr1, sdslen(rr1));
                    break;

                case ft_INFO1:
                    INFO("Writing data packet to file\n\n");
                    write(file, data, sdslen(data));
                    write(ll.fd, rr0, sdslen(rr0));
                    break;

                default:
                    break;
            }
        }
    }

    sdsfree(ua);
    sdsfree(rr0);
    sdsfree(rr1);
    sdsfree(disc);

    sdsfree(data);
    sdsfree(info_buf);

    return terminate_connection;
}

int llclose(linkLayer ll, int showStatistics) {
    if (ll.role == RECEIVER) {
        serial_close(ll.fd, &ll.oldtio);
        return 0;
    }

    sds disc = sdsnewframe(ft_DISC);
    bool success = send_frame(ll, disc, ft_DISC);
    sdsfree(disc);

    if (success)
        INFO("Connection complete\n");
    else
        ERROR("Disconnect failure\n");

    serial_close(ll.fd, &ll.oldtio);

    return success - 1;
}

//* ------------------------------------------------------------------
//* ------------------------ AUXILIARY FUNCTIONS ---------------------
//* ------------------------------------------------------------------

void alarm_handler(int signum)  // atende alarme
{
    alarm_counter++;
    alarm_flag = true;
    ERROR("Alarm Interrupt Triggered with code %d - Attempt %d\n", signum, alarm_counter);
}

int serial_open(char* serial_port) {
    int fd;

    fd = open(serial_port, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        ERROR("%s", serial_port);
        exit(-1);
    }

    return fd;
}

int serial_config(linkLayer* ll) {
    struct termios newtio;

    LOG("SAVING terminal settings\n");

    sleep(1);
    if (tcgetattr(ll->fd, &ll->oldtio) == -1) { /* save current port settings */
        ERROR("tcgetattr failed on serial_config");
        return -1;
    }

    LOG("Settings saved\n");

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = ll->baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    //* VTIME - Timeout in deciseconds for noncanonical read.
    //* VMIN - Minimum number of characters for noncanonical read.
    // VTIME = 1 para esperar 100 ms por read
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

    tcflush(ll->fd, TCIOFLUSH);

    if (tcsetattr(ll->fd, TCSANOW, &newtio) == -1) {
        ERROR("tcsetattr failed on serial_config");
        return -1;
    }

    LOG("New termios structure set.\n");

    return 0;
}

void serial_close(int fd, struct termios* configs) {
    if (tcsetattr(fd, TCSANOW, configs) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}

bool send_frame(linkLayer ll, sds packet, frame_type ft_expected) {
    uchar rcved;

    frame_type ft_detected = ft_ANY;
    frame_state fs_current = fs_FLAG1;

    bool success = false;

    while (alarm_counter < ll.numTries) {
        write(ll.fd, packet, sdslen(packet));

        alarm(ll.timeOut);
        while (true) {
            if (alarm_flag)
                break;

            int nbytes = read(ll.fd, &rcved, sizeof rcved);
            if (!nbytes)
                continue;
            fs_current = frame_handler(fs_current, &ft_detected, rcved);

            if (ft_detected == ft_expected && fs_current == fs_VALID) {
                alarm(0);
                alarm_counter = 0;
                success = true;
                break;
            }
        }
        if (success)
            break;

        alarm_flag = false;
    }
    return success;
}

uchar calculate_bcc2(sds data) {
    uchar bcc2 = data[0];
    for (int i = 1; i < sdslen(data); i++)
        bcc2 = bcc2 ^ data[i];
    return bcc2;
}

uchar validate_bcc2(sds data) {
    int bcc2_pos = (int)sdslen(data) - 1;
    LOG("---------------\n");
    LOG("Validating BCC2\n");
    uchar bcc2 = data[0];
    uchar bcc2_expected = data[bcc2_pos];

    // LOG("POS 0: 0x%.02x = '%c'\n", (unsigned int)(bcc2 & 0xFF), bcc2);
    for (int i = 1; i < bcc2_pos; i++) {
        // LOG("POS %d: 0x%.02x = '%c'\n", i, (unsigned int)(data[i] & 0xFF), data[i]);
        bcc2 = bcc2 ^ data[i];
    }

    // ALERT("STRING LENGHT: %ld\n", strlen(data));
    LOG("Calculated BCC2: 0x%.02x = '%c'\n", (unsigned int)(bcc2 & 0xFF), bcc2);
    LOG("Expected BCC2: 0x%.02x = '%c'\n", (unsigned int)(bcc2_expected & 0xFF), bcc2_expected);
    return bcc2 == bcc2_expected;
}