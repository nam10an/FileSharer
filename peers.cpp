#include <fstream>
#include "peers.hpp"
#include<iostream>
std::mutex mtx;
struct sockaddr_in server_address;


int client::calculate_chunk_number(uint32_t file_length){
    int chunk_number = file_length / CHUNK_SIZE;
    //round up
    if((CHUNK_SIZE * chunk_number) < file_length){
        chunk_number++;
    }
    return chunk_number;
}

int client::register_request(uint16_t number_of_file, std::vector<file_info> file_infos){
    /*
    send:
        request type
        the port number a peer is listening to
        number of file a peer want to share
        a list of file name
        a list of file length
    receive:
        OK
    */
    int peer_fd;
    char receiveMessage[100] = {};
    uint32_t file_length = 0;
    std::string file_name;
    const uint32_t request_type = 0;
    uint16_t port = this->port;
    std::string response_message;

    if((peer_fd = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket"); 
        exit(0);
    }

    if(connect(peer_fd,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect"); 
        exit(0);
    }
    
    send(peer_fd, &request_type, sizeof(request_type), 0);
    send(peer_fd, &port, sizeof(port), 0);
    send(peer_fd, &number_of_file, sizeof(number_of_file), 0);
    for(int i=0; i < number_of_file; i++){
        send(peer_fd, &file_infos[i].file_name, sizeof(file_infos[i].file_name), 0);
        send(peer_fd, &file_infos[i].file_length, sizeof(file_infos[i].file_length), 0);
    }
    if(recv(peer_fd, &response_message, sizeof(response_message), 0) < 0){
        perror("server response failed");
        return -1;
    }
    std::cout << "server response:" << response_message << std::endl;
    close(peer_fd);
    return 0;
}

int client::file_list_request(){
    /*
    send:
        request type
    receive:
        number of file a client can download
        a list of file name
        a list of file length
    */
    int peer_fd;
    const uint32_t request_type = 1;
    uint16_t number_of_file;
    std::string file_name;
    uint32_t file_length;

    if((peer_fd = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket"); 
        exit(0);
    }

    if(connect(peer_fd,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect"); 
        exit(0);
    }

    send(peer_fd, &request_type, sizeof(request_type), 0);
    recv(peer_fd, &number_of_file, sizeof(number_of_file), 0);
    for(int i = 0; i < number_of_file; i++){
        recv(peer_fd, &file_name, sizeof(file_name), 0);
        recv(peer_fd, &file_length, sizeof(file_length), 0);
        std::cout << file_name << ": "<< file_length <<" bytes";
        if(i != number_of_file - 1){
            std::cout<<", ";
        }
    }
    std::cout << std::endl;
    close(peer_fd);

    return 0;
}

int client::location_request(std::string file_name, std::vector<chunk>& chunks, int & file_size){
    /*
    send:
        request type
        file name
    receive:
        number_of_location
        file_length
        a list of ip_address
        a list of port
        a list of number_of_chunks
        a list of chunk index for every ip_address

    */
    int peer_fd;
    const uint32_t request_type = 2;
    uint16_t number_of_location;
    uint16_t number_of_chunk;
    uint32_t ip_address;
    uint16_t port;
    struct in_addr temp_address;
    uint32_t chunk_index;
    uint32_t file_length;
    //std::vector<chunk> chunks;
    struct chunk temp_chunk;
    struct peer temp_peer;

    if((peer_fd = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket"); 
        exit(0);
    }

    if(connect(peer_fd,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect"); 
        exit(0);
    }

    send(peer_fd, &request_type, sizeof(request_type), 0);
    send(peer_fd, &file_name, sizeof(file_name), 0);
    recv(peer_fd, &number_of_location, sizeof(number_of_location), 0);

    if(number_of_location == 0){
        std::cout<<"file not found"<<std::endl;
        return -1;
    }

    recv(peer_fd, &file_length, sizeof(file_length), 0);
    file_size = file_length;
    int chunk_number = calculate_chunk_number(file_length);
    for(int i = 0; i<chunk_number; i++){
        temp_chunk.index = i;
        temp_chunk.is_possessed = false;
        chunks.push_back(temp_chunk);
    }

    for(int i=0; i<number_of_location; i++){
        recv(peer_fd, &ip_address, sizeof(ip_address), 0);
        recv(peer_fd, &port, sizeof(port), 0);
        temp_address.s_addr = ip_address;

        recv(peer_fd, &number_of_chunk, sizeof(number_of_chunk), 0);
        for(int j=0; j<number_of_chunk; j++){
            recv(peer_fd, &chunk_index, sizeof(chunk_index), 0);
            for(int k = 0; k<chunks.size(); k++){
                if(chunks[k].index == chunk_index){
                    temp_peer.address.s_addr = ip_address;
                    temp_peer.port = port;
                    chunks[k].peers.push_back(temp_peer);
                }
            }
        }
    }
    close(peer_fd);
    return 0;
}

void client::split(const std::string& s, std::vector<std::string>& parameters, const char delim = ' ') {
    parameters.clear();
    std::istringstream iss(s);
    std::string temp;

    while (std::getline(iss, temp, delim)) {
        parameters.emplace_back(std::move(temp));
    }
    return;
}

bool client::file_exist(std::string file_name){
    struct stat buffer;   
    return (stat (file_name.c_str(), &buffer) == 0); 
}

int client::write_log(std::string file_name, std::vector<chunk> chunks){
    std::ofstream log;
    log.open((file_name + ".log").c_str());
    for(int i = 0; i < chunks.size(); i++){
        if(chunks[i].is_possessed == true){
            log<<chunks[i].index<<"\n";
        }
    }
    log.close();
    return 0;
}

int client::read_log(std::string file_name, std::vector<chunk>& chunks, int file_length){
    std::ifstream log;
    std::ofstream download_file;
    char buffer[10];
    std::string log_line;
    std::vector<std::string> elements;

    
    if(!file_exist(file_name + ".log")){
        return 0;
    }
    

    log.open((file_name + ".log").c_str());
    
    while(log.getline(buffer, 10)){
        log_line = buffer;
        std::stringstream s(log_line);
        int index = 0;
        s >> index;
        chunks[index].is_possessed = true;
        std::stringstream().swap(s);
    }
    return 0;
}

std::vector<int> client::select_chunk(std::vector<chunk> chunks){
    std::vector<int> min;
    int min_number = INT_MAX;
    int min_index = 0;
    bool flag = false;
    for(int i = 0; i<4; i++){
        for(int j=0; j<chunks.size(); j++){
            if((chunks[j].peers.size() < min_number) && (!chunks[j].is_possessed)){
                min_number = chunks[j].peers.size();
                min_index = j;
                flag = true;
            }
        }
        if(flag){
            min.push_back(min_index);
            chunks[min_index].is_possessed = true;
            min_number = INT_MAX;
            flag = false;
        }
    }
    return min;
}

//change 4
int client::download_file(std::string remote_name, std::string local_name, int index, struct chunk chunk_instance){
    int sock_fd;
    char buffer[1024];
    struct sockaddr_in peer_address;
    int selected_peer_index = rand() % chunk_instance.peers.size();

    // initialize address structure
    bzero(&peer_address, sizeof(peer_address));
    peer_address.sin_family = PF_INET;
    peer_address.sin_addr.s_addr = chunk_instance.peers[selected_peer_index].address.s_addr;
    peer_address.sin_port = htons(chunk_instance.peers[selected_peer_index].port);

    std::cout << "LIST" << std::endl;
    std::cout << "peer: " << inet_ntoa(chunk_instance.peers[selected_peer_index].address)
              << ":" << chunk_instance.peers[selected_peer_index].port
              << " chunk: " << index << std::endl;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket"); 
        exit(0);
    }

    if (connect(sock_fd, (struct sockaddr*)&peer_address, sizeof(peer_address)) < 0) {
        perror("Failed to connect"); 
        exit(0);
    }

    send(sock_fd, &remote_name, sizeof(remote_name), 0);
    send(sock_fd, &index, sizeof(index), 0);

    // receive chunk data
    // recv(sock_fd, &buffer, sizeof(buffer), 0);
    int received_size = recv(sock_fd, buffer, sizeof(buffer), 0);
    close(sock_fd);
    if (received_size <= 0) {
        std::cerr << "Failed to receive data from peer.\n";
        close(sock_fd);
        return -1;
    }
    // write chunk to file using local_name
    std::ofstream download;
    std::stringstream ss;
    std::string index_str;
    int chunk_size = CHUNK_SIZE;

    ss << index;
    ss >> index_str;

    mtx.lock();
    std::cout << "Saving to chunk file: " << local_name + index_str << " (" << received_size << " bytes)\n";
    download.open(local_name + index_str, std::ios::binary);  // write chunk as e.g. downloaded_demo.txt0
    // download.write(buffer, strlen(buffer));  // write only actual bytes (for now)
    download.write(buffer, received_size);
    download.close();
    mtx.unlock();

    return 0;
}




//change 1
int client::combine_file(std::string file_name, int chunk_number){
    std::ifstream read;
    std::ofstream write(file_name, std::ios::binary);  // open in binary mode
    std::string temp_file_name;
    std::string index_string;
    // std::stringstream ss;
    char buffer[CHUNK_SIZE];

    for(int i = 0; i < chunk_number; i++){
        // ss << i;
        // ss >> index_string;
        std::stringstream ss;
        ss << i;
        std::string index_string = ss.str();
        temp_file_name = file_name + index_string; 
        read.open(temp_file_name, std::ios::binary);  // read in binary mode
        if (!read.is_open()) {
            std::cerr << "Failed to open chunk file: " << temp_file_name << "\n";
            continue;
        }
        read.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = read.gcount();  // only actual data
        write.write(buffer, bytes_read);
        std::stringstream().swap(ss);
        read.close();
        remove(temp_file_name.c_str());  // clean up the chunk
    }
    write.close();
    return 0;
}

int client::register_chunk(std::string file_name, int index){
    int peer_fd;
    std::string file = file_name;
    int chunk_index = index;
    std::string response_message;

    if((peer_fd = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket"); 
        return -1;
    }

    if(connect(peer_fd,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect"); 
        return -1;
    }

    uint32_t request_type = 3;
    uint16_t port = this->port;
    send(peer_fd, &request_type, sizeof(request_type), 0);
    send(peer_fd, &file, sizeof(file), 0);
    send(peer_fd, &chunk_index, sizeof(chunk_index), 0);
    send(peer_fd, &port, sizeof(port), 0);

    if(recv(peer_fd, &response_message, sizeof(response_message), 0) < 0){
        perror("No response from server");
        return -1;
    }
    std::cout<<response_message<<std::endl;
    return 0;
}

//change
int client::download(std::string remote_name, std::string local_name) {
    client myclient;
    std::vector<chunk> chunks;
    std::vector<int> indexes;
    std::vector<std::thread> download_threads;
    int file_length;

    location_request(remote_name, chunks, file_length);

    read_log(local_name, chunks, file_length);

    indexes = select_chunk(chunks);

    while (!indexes.empty()) {
        for (int i = 0; i < indexes.size(); i++) {
            // Start download in a new thread (store it for joining later)
            download_threads.emplace_back(
                &client::download_file,
                myclient,
                remote_name,
                local_name,
                indexes[i],
                chunks[indexes[i]]
            );
            chunks[indexes[i]].is_possessed = true;

            register_chunk(remote_name, indexes[i]);
        }
        indexes = select_chunk(chunks);
    }

    for (auto& t : download_threads) {
        if (t.joinable()) t.join();
    }

    write_log(local_name, chunks);
    combine_file(local_name, chunks.size());

    for (int i = 0; i < chunks.size(); i++) {
        std::cout << "chunk" << i << ":\n";
        for (int j = 0; j < chunks[i].peers.size(); j++) {
            std::cout << "ip:" << inet_ntoa(chunks[i].peers[j].address)<< ":" << chunks[i].peers[j].port << "\n";
        }
    }

    return 0;
}

long client::get_file_size(std::string filename){
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

int client::parse_command(std::string command){
    std::vector<std::string> parameters;
    std::vector<file_info> file_infos;

    split(command, parameters);
    if(parameters[0] == "show"){
        file_list_request();
        return 0;
    }
    else if(parameters[0] == "share"){
        std::stringstream file_number_string(parameters[1]);
        int file_number = 0;
        struct file_info temp_file_info;
        file_number_string >> file_number;
        for(int i=0; i<file_number; i++){
            temp_file_info.file_name = parameters[i+2];
            temp_file_info.file_length = get_file_size(parameters[i+2]);
            file_infos.push_back(temp_file_info);
        }
        register_request(file_number, file_infos);
    }
    else if(parameters[0] == "download"){
        // download(parameters[1]);

        //change 3
        std::string remote_file = parameters[1];
        std::string local_file = "downloaded_" + remote_file;
        download(remote_file, local_file);
    }
    else{
        std::cout<<"command not found"<<std::endl;
    }
    return 0;
}

void client::user_interface(){
    char buffer[1024];
    while(1){
        std::string command;
        std::cout<<">";
        std::cin.getline(buffer, 1024);
        std::cout << "[DEBUG] Command received: " << buffer << "\n";
        command = buffer;
        parse_command(command);
    }
    return;
}

void client::handle_request(int sock_fd, std::string file_name, int chunk_index) {
    std::ifstream file(file_name, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open requested file: " << file_name << "\n";
        close(sock_fd);
        return;
    }

    file.seekg(chunk_index * CHUNK_SIZE);

    char buffer[CHUNK_SIZE] = {0}; 
    file.read(buffer, CHUNK_SIZE);
    int bytes_read = file.gcount();  
    send(sock_fd, buffer, bytes_read, 0);

    file.close();
    close(sock_fd);
}

int client::execute(void) {
    std::cout << "[DEBUG] Listening on port " << this->port << "\n";

    int process_fd;
    int server_fd;
    struct sockaddr_in client_address, peer_address;

    srand(time(NULL));

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = PF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_port = htons(SERVER_PORT);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");
        return -1;
    }

    bzero(&peer_address, sizeof(peer_address));
    peer_address.sin_family = PF_INET;
    peer_address.sin_addr.s_addr = INADDR_ANY;
    peer_address.sin_port = htons(this->port); 

    if (bind(server_fd, (struct sockaddr*)&peer_address, sizeof(peer_address)) < 0) {
        perror("Failed to bind");
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Failed to listen");
        return -1;
    }

    std::cout << "Listening on port " << this->port << std::endl;

    while (1) {
        socklen_t client_address_length = sizeof(client_address);

        if ((process_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_address_length)) < 0) {
            perror("Failed to accept");
            continue;
        }

        std::string file_name;
        int chunk_index;
        uint32_t request_type;

        recv(process_fd, &file_name, sizeof(file_name), 0);
        recv(process_fd, &chunk_index, sizeof(chunk_index), 0);

        std::cout << "upload file: " << file_name << ", " << chunk_index << std::endl;

        std::thread tid(&client::handle_request, this, process_fd, file_name, chunk_index);
        tid.detach();
    }

    close(server_fd);
    return 0;
}
