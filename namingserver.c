#include "header.h"

int main()
{
    Tree SS1 = MakeNode(".");
    storage_server_list = NULL;
    int nm_sock, client_sock, ss_sock;
    struct sockaddr_in server_addr, client_addr, ss_addr;
    socklen_t client_addr_size, ss_addr_size;

    ss_addr_size = sizeof(ss_addr);

    open_naming_server_port(5566, &nm_sock, &server_addr);
    make_socket_non_blocking(nm_sock); // so that some accept requests can be ignored

    // we now have a dedicated port for the naming server
    int ns_sock;
    struct sockaddr_in ns_addr;

    int something_connect = 0;
    int num_ss = 0;
    int num_client = 0;
    int role = 0;

    while (1)
    {
        ss_sock = accept(nm_sock, (struct sockaddr *)&ss_addr, &ss_addr_size);
        if (ss_sock < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                // No client connection available, continue with other tasks
            }
            else
            {
                perror(RED "[-]Accept error" RESET);
                exit(0);
            }
        }
        else
        {
            if (recv(ss_sock, &role, sizeof(role), 0) == -1)
            {
                perror(RED "[-]Receive error" RESET);
                exit(1);
            }
            else
            {
                printf("role is %d\n", role);
                something_connect = 1;
                if (role == 1)
                {
                    num_ss++;
                    if (initialize_SS(&ss_sock) == -1)
                    {
                        perror(RED "[-]Error initializing storage servers" RESET);
                        exit(1);
                    }
                    printf("[+]New storage server connected\n");
                    close_socket(&ss_sock);
                    continue;
                }
                else if (role == 2)
                {
                    client_sock = ss_sock;
                    client_addr = ss_addr;
                    client_addr_size = ss_addr_size;
                    num_client++;
                    printf("[+]New client connected\n");
                }
            }
            // receive vital information, store in ll, disconnect
        }
        if (something_connect == 0 || (something_connect != 0 && num_client == 0))
        {
            continue;
        }
        if (num_ss == 0 && num_client != 0)
        {
            printf(RED "[-]No storage servers connected\n" RESET);
            break;
            continue;
        }

        PrintAll();
        char opt[2];
        int recieved;
        if ((recieved = recv(client_sock, &opt, sizeof(opt), 0)) == -1)
        {
            perror(RED "[-]Receive Error" RESET);
            close(client_sock);
            exit(1);
        }
        else if (recieved == 0)
        {
            // The client has closed the connection, so break out of the loop
            printf(RED "Client disconnected.\n" RESET);
            close(client_sock);
            break;
        }
        opt[strlen(opt)] = '\0';

        if (strcmp("1", opt) == 0)
        {
            close_socket(&client_sock);
        }
        else if (strcmp("2", opt) == 0) // Deletion
        {
            char temp_file_path[MAX_FILE_PATH];
            char temp_option[10];

            // Receiving the path of the file/directory
            char file_path[MAX_FILE_PATH];
            if ((recieved = recv(client_sock, &file_path, sizeof(file_path), 0)) == -1)
            {
                perror(RED "Not successful" RESET);
                exit(0);
            }

            // Recieving the create option - 1 for file and 2 for directory
            char create_option[10];
            if ((recieved = recv(client_sock, &create_option, sizeof(create_option), 0)) == -1)
            {
                perror(RED "Not successful" RESET);
                exit(0);
            }

            // END OF GETTING DATA FROM CLIENT
            // THE REST OF THIS CODE MUST EXECUTE ONLY IF file_path IS IN THE LIST OF ACCESSIBLE PATHS

            storage_servers storage_server_details = check_if_path_in_ss(file_path, 0);
            if (storage_server_details == NULL)
            {
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                continue;
            }

            if (Delete_Path(storage_server_details->files_and_dirs, file_path) == -1)
            {
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                continue;
            }
            if (send(client_sock, "success", sizeof("success"), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            printf("Waiting for success message\n");

            connect_to_SS_from_NS(&ns_sock, &ns_addr, storage_server_details->ss_send->server_port);
            if (send(ns_sock, "2", sizeof("2"), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Sending path to the SS
            if (send(ns_sock, file_path, sizeof(file_path), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            //  Sending option to the SS

            if (send(ns_sock, create_option, sizeof(create_option), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Checking if creation was successful
            char success[20];
            int success_message = 0;

            if ((success_message = recv(ns_sock, &success, sizeof(success), 0)) == -1)
            {
                perror(RED "[-]Not successful" RESET);
                exit(0);
            }
            else
                success[strlen(success)] = '\0';

            if (strcmp(success, "done") == 0)
            {
                printf(GREEN "Deleted Successfully!\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            else
            {
                printf(RED "[-]Deletion unsuccessful\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            close_socket(&ns_sock);
        }
        else if (strcmp("3", opt) == 0) // Creation
        {
            char temp_file_path[MAX_FILE_PATH];
            char temp_option[10];

            // Receiving the path of the file/directory
            char file_path[MAX_FILE_PATH];
            if ((recieved = recv(client_sock, &file_path, sizeof(file_path), 0)) == -1)
            {
                perror(RED "[-]Receive error\n" RESET);
                exit(1);
            }

            // Recieving the create option - 1 for file and 2 for directory
            char create_option[10];
            if ((recieved = recv(client_sock, &create_option, sizeof(create_option), 0)) == -1)
            {
                perror(RED "[-]Receive error\n" RESET);
                exit(1);
            }

            // END OF GETTING DATA FROM CLIENT
            // THE REST OF THIS CODE MUST EXECUTE ONLY IF file_path IS IN THE LIST OF ACCESSIBLE PATHS

            storage_servers storage_server_details = check_if_path_in_ss(file_path, 1);
            if (storage_server_details == NULL)
            {
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }

                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                continue;
            }
            else
            {
                if (send(client_sock, "success", sizeof("success"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }

            connect_to_SS_from_NS(&ns_sock, &ns_addr, storage_server_details->ss_send->server_port);
            if (send(ns_sock, "3", sizeof("3"), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Sending path to the SS
            if (send(ns_sock, file_path, sizeof(file_path), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            //  Sending option to the SS
            if (send(ns_sock, create_option, sizeof(create_option), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Checking if creation was successful
            char success[25];
            int success_message = 0;

            if ((success_message = recv(ns_sock, &success, sizeof(success), 0)) == -1)
            {
                perror(RED "[-]Receive error\n" RESET);
                return 1;
            }
            else
                success[strlen(success)] = '\0';

            if (strcmp(success, "done") == 0)
            {
                printf(GREEN "Created Successfully!\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            else
            {
                printf(RED "[-]%s\n" RESET, success);
                perror(RED "[-]Creation unsuccessful\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            close_socket(&ns_sock);
        }
        else if (strcmp("4", opt) == 0) // Copying files/directories
        {

            // Receiving the path of the file/directory
            char source_path[MAX_FILE_PATH];
            char dest_path[MAX_FILE_PATH];
            if ((recieved = recv(client_sock, &source_path, sizeof(source_path), 0)) == -1)
            {
                perror(RED "Not successful" RESET);
                exit(0);
            }
            if ((recieved = recv(client_sock, &dest_path, sizeof(dest_path), 0)) == -1)
            {
                perror(RED "Not successful" RESET);
                exit(0);
            }

            // Recieving the create option - 1 for file and 2 for directory
            char copy_option[10];
            if ((recieved = recv(client_sock, &copy_option, sizeof(copy_option), 0)) == -1)
            {
                perror(RED "Not successful" RESET);
                exit(0);
            }
            // Checking if destination is accessible
            storage_servers storage_server_details = check_if_path_in_ss(dest_path, 0);

            if (storage_server_details == NULL)
            {
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }

                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                continue;
            }
            // Checking if source is accessible
            storage_server_details = check_if_path_in_ss(source_path, 0);

            if (storage_server_details == NULL)
            {
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }

                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                continue;
            }
            else
            {
                if (send(client_sock, "success", sizeof("success"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }

            // Sending data to SS

            connect_to_SS_from_NS(&ns_sock, &ns_addr, storage_server_details->ss_send->server_port);
            if (send(ns_sock, "4", sizeof("4"), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Sending source path to the SS
            if (send(ns_sock, source_path, sizeof(source_path), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            // Sending destination path to the SS
            if (send(ns_sock, dest_path, sizeof(dest_path), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            //  Sending option to the SS
            if (send(ns_sock, copy_option, sizeof(copy_option), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }
            char success[25];
            int success_message = 0;

            if ((success_message = recv(ns_sock, &success, sizeof(success), 0)) == -1)
            {
                perror(RED "[-]Receive error\n" RESET);
                return 1;
            }
            else
                success[strlen(success)] = '\0';

            if (strcmp(success, "done") == 0)
            {
                printf(GREEN "Copied Successfully!\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            else
            {
                printf(RED "[-]%s\n" RESET, success);
                perror(RED "[-]Copied unsuccessful\n" RESET);
                if (send(client_sock, success, sizeof(success), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            close_socket(&ns_sock);
        }
        else if (strcmp("5", opt) == 0 || strcmp("6", opt) == 0 || strcmp("7", opt) == 0) // Write
        {
            char file_path[MAX_FILE_PATH];
            if ((recieved = recv(client_sock, &file_path, sizeof(file_path), 0)) == -1)
            {
                perror(RED "[-]Receive error\n" RESET);
                exit(1);
            }

            storage_servers storage_server_details = check_if_path_in_ss(file_path, 0);
            if (storage_server_details == NULL)
            {
                printf(RED "[-]Path not in list of accessible paths\n" RESET);
                if (send(client_sock, "failed", sizeof("failed"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
                continue;
            }

            int server_addr = storage_server_details->ss_send->client_port;
            char ip_addr[50];
            strcpy(ip_addr, storage_server_details->ss_send->ip_addr);
            strcpy(ip_addr, "127.0.0.1"); // FIX
            char server[50];
            snprintf(server, sizeof(server), "%d", server_addr);
            if (send(client_sock, ip_addr, sizeof(ip_addr), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }
            if (send(client_sock, server, sizeof(server), 0) == -1)
            {
                perror(RED "[-]Send error\n" RESET);
                exit(1);
            }

            connect_to_SS_from_NS(&ns_sock, &ns_addr, storage_server_details->ss_send->server_port);
            if (strcmp("5", opt) == 0)
            {
                if (send(ns_sock, "5", sizeof("5"), 0) == -1)
                {
                    perror(RED "[-]Send error\n" RESET);
                    exit(1);
                }
            }
            else if (strcmp("6", opt) == 0)
            {
                if (send(ns_sock, "6", sizeof("6"), 0) == -1)
                {
                    perror(RED "[-]Send error" RESET);
                    exit(1);
                }
            }
            else if (strcmp("7", opt) == 0)
            {
                if (send(ns_sock, "7", sizeof("7"), 0) == -1)
                {
                    perror(RED "[-]Send error" RESET);
                    exit(1);
                }
            }
            close_socket(&ns_sock);
        }
    }
    // close_socket(&ns_sock); // Already being closed
    // close_socket(&client_sock); // I am commenting this out because the client socket should already be closed
    close_socket(&nm_sock);

    return 0;
}