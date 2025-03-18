#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
// Don't redefine closesocket as close - we'll handle it differently
#endif

// Wave file header structure
struct WavHeader {
  // RIFF chunk
  char riff_header[4] = {'R', 'I', 'F', 'F'};
  uint32_t wav_size = 0; // Will be filled in later
  char wave_header[4] = {'W', 'A', 'V', 'E'};

  // fmt chunk
  char fmt_header[4] = {'f', 'm', 't', ' '};
  uint32_t fmt_chunk_size = 16;
  uint16_t audio_format = 1; // PCM
  uint16_t num_channels = 1; // Mono
  uint32_t sample_rate = 16000;
  uint32_t byte_rate = 32000; // sample_rate * num_channels * bytes_per_sample
  uint16_t block_align = 2;   // num_channels * bytes_per_sample
  uint16_t bits_per_sample = 16;

  // data chunk
  char data_header[4] = {'d', 'a', 't', 'a'};
  uint32_t data_chunk_size = 0; // Will be filled in later
};

class UDPClient {
public:
  UDPClient(const std::string &server_ip = "192.168.4.1",
            int server_port = 5001)
      : server_ip(server_ip), server_port(server_port), running(false),
        connected(false), total_bytes(0), bytes_since_last_update(0),
        sample_rate(16000) {

    // Create timestamped filename
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm *now_tm = std::localtime(&now_time);

    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", now_tm);
    wav_filename = "audio_" + std::string(timestamp) + ".wav";

// Initialize socket
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      std::cerr << "Failed to initialize Winsock" << std::endl;
      exit(1);
    }
#endif

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
      std::cerr << "Failed to create socket" << std::endl;
#ifdef _WIN32
      WSACleanup();
#endif
      exit(1);
    }

// Set socket timeout
#ifdef _WIN32
    DWORD timeout = 1000; // 1 second
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
               sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#endif

    init_wav_file();
  }

  ~UDPClient() { close(); }

  void init_wav_file() {
    if (wav_file.is_open()) {
      wav_file.close();
    }

    wav_file.open(wav_filename, std::ios::binary);
    if (!wav_file.is_open()) {
      std::cerr << "Failed to create WAV file: " << wav_filename << std::endl;
      exit(1);
    }

    // Write header (will update sizes later)
    WavHeader header;
    wav_file.write(reinterpret_cast<const char *>(&header), sizeof(header));
    data_size = 0;

    std::cout << "Created new WAV file: " << wav_filename << std::endl;
  }

  bool connect() {
    std::cout << "Trying to connect to " << server_ip << ":" << server_port
              << "..." << std::endl;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
#ifdef _WIN32
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
#else
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
#endif

    try {
      // Send initialization packet
      const char *hello_msg = "hello";
      if (sendto(sock, hello_msg, strlen(hello_msg), 0,
                 (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: sendto error" << std::endl;
        return false;
      }
      return true;
    } catch (const std::exception &e) {
      std::cerr << "Connection failed: " << e.what() << std::endl;
      return false;
    }
  }

  bool start_receiving() {
    if (!connect()) {
      return false;
    }

    running = true;
    connected = true;

    // Start receive thread
    receive_thread = std::thread(&UDPClient::_receive_loop, this);
    stats_thread = std::thread(&UDPClient::_stats_loop, this);

    return true;
  }

  void close() {
    running = false;

    if (receive_thread.joinable()) {
      receive_thread.join();
    }

    if (stats_thread.joinable()) {
      stats_thread.join();
    }

    if (wav_file.is_open()) {
      // Update WAV header with final sizes
      wav_file.seekp(0, std::ios::end);
      size_t file_size = wav_file.tellp();

      // Update RIFF chunk size
      wav_file.seekp(4, std::ios::beg);
      uint32_t riff_size = file_size - 8;
      wav_file.write(reinterpret_cast<const char *>(&riff_size),
                     sizeof(riff_size));

      // Update data chunk size
      wav_file.seekp(40, std::ios::beg);
      wav_file.write(reinterpret_cast<const char *>(&data_size),
                     sizeof(data_size));

      wav_file.close();
      std::cout << "\nSaved audio file: " << wav_filename << std::endl;
    }

// Close socket with platform-specific method
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    ::close(sock);
#endif
  }

  std::string get_wav_filename() const { return wav_filename; }

  std::string get_server_ip() const { return server_ip; }

  int get_server_port() const { return server_port; }

private:
  void _stats_loop() {
    auto last_update_time = std::chrono::steady_clock::now();

    while (running) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(200)); // Update every 0.2 seconds

      auto current_time = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         current_time - last_update_time)
                         .count() /
                     1000.0;

      double bytes_per_second =
          (elapsed > 0) ? static_cast<double>(bytes_since_last_update) / elapsed
                        : 0;

      // Calculate audio duration (seconds)
      double audio_duration = static_cast<double>(total_bytes) /
                              (sample_rate * 2); // 2 bytes/sample

      // Show statistics (print without newline)
      std::cout << "\rReceived: " << std::fixed << std::setprecision(1)
                << (total_bytes / 1024.0) << "KB ("
                << (bytes_per_second / 1024.0) << " KB/s) | "
                << "Duration: " << audio_duration << "s" << std::flush;

      last_update_time = current_time;
      bytes_since_last_update = 0;
    }
  }

  void _receive_loop() {
    const size_t buffer_size = 2048;
    char buffer[buffer_size];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_size = sizeof(sender_addr);

    while (running) {
      try {
        int received_bytes =
            recvfrom(sock, buffer, buffer_size, 0,
                     (struct sockaddr *)&sender_addr, &sender_addr_size);

        if (received_bytes > 0) {
          // Process as int16 data (similar to numpy.frombuffer)
          int16_t *int16_data = reinterpret_cast<int16_t *>(buffer);
          int sample_count = received_bytes / 2; // Each sample is 2 bytes

          // Check for leading zeros and skip if necessary
          int start_idx = 0;
          if (sample_count >= 482 && int16_data[0] == 0 && int16_data[1] == 0) {
            start_idx = 2;
            std::cout << "\nSkipped 2 leading zeros, remaining samples: "
                      << (sample_count - 2) << std::endl;
          }

          // Print first 8 samples
          if (sample_count > start_idx) {
            std::cout << "First 8 samples: ";
            for (int i = start_idx; i < std::min(start_idx + 8, sample_count);
                 i++) {
              std::cout << int16_data[i] << " ";
            }
            std::cout << std::endl;

            // Calculate data range statistics
            int16_t min_val = 32767, max_val = -32768;
            double sum = 0;
            for (int i = start_idx; i < sample_count; i++) {
              min_val = std::min(min_val, int16_data[i]);
              max_val = std::max(max_val, int16_data[i]);
              sum += int16_data[i];
            }
            double mean = sum / (sample_count - start_idx);

            std::cout << "Data range: min=" << min_val << ", max=" << max_val
                      << ", mean=" << std::fixed << std::setprecision(2) << mean
                      << std::endl;
          }

          // Only write valid audio data (skip the first two samples if needed)
          int data_size_to_write = (sample_count - start_idx) * 2;
          if (data_size_to_write > 0) {
            wav_file.write(
                reinterpret_cast<const char *>(&int16_data[start_idx]),
                data_size_to_write);

            // Update statistics
            total_bytes += data_size_to_write;
            bytes_since_last_update += data_size_to_write;
            data_size += data_size_to_write;
          }
        }
      } catch (const std::exception &e) {
        std::cerr << "\nError receiving data: " << e.what() << std::endl;
        std::this_thread::sleep_for(
            std::chrono::seconds(1)); // Wait one second after an error
      }
    }
  }

  std::string server_ip;
  int server_port;
  SOCKET sock;
  std::atomic<bool> running;
  std::atomic<bool> connected;
  std::thread receive_thread;
  std::thread stats_thread;

  int sample_rate;
  std::string wav_filename;
  std::ofstream wav_file;
  uint32_t data_size;

  std::atomic<size_t> total_bytes;
  std::atomic<size_t> bytes_since_last_update;
};

// Signal handler for Ctrl+C
UDPClient *global_client = nullptr;
void signal_handler(int signal) {
  std::cout << "\nStopping recording..." << std::endl;
  if (global_client) {
    global_client->close();
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  std::string server_ip = "192.168.4.1";

  if (argc > 1) {
    server_ip = argv[1];
  }

  UDPClient client(server_ip);
  global_client = &client;

  // Set up signal handler for clean termination
  signal(SIGINT, signal_handler);

  try {
    if (!client.start_receiving()) {
      std::cout << "Failed to start client" << std::endl;
      return 1;
    }

    std::cout << "UDP Client started, connecting to " << client.get_server_ip()
              << ":" << client.get_server_port() << std::endl;
    std::cout << "Audio will be saved to: " << client.get_wav_filename()
              << std::endl;
    std::cout << "Press Ctrl+C to stop recording..." << std::endl;

    // Keep main thread alive
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  client.close();
  return 0;
}
