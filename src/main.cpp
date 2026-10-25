#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef CINEBOOK_WEB_DIR
#define CINEBOOK_WEB_DIR "web"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
static constexpr Socket invalid_socket = INVALID_SOCKET;
static void close_socket(Socket socket) { closesocket(socket); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
static constexpr Socket invalid_socket = -1;
static void close_socket(Socket socket) { close(socket); }
#endif

namespace {
struct Movie {
    int id;
    const char* title;
    const char* genre;
    const char* rating;
    const char* duration;
    const char* poster;
    const char* description;
};

const std::array<Movie, 6> movies{{
    {1, "Dune: Part Two", "Sci-Fi · Adventure", "8.8", "2h 46m", "dune",
     "Paul Atreides unites with Chani and the Fremen while seeking revenge against the conspirators who destroyed his family."},
    {2, "Poor Things", "Fantasy · Romance", "8.1", "2h 21m", "poor-things",
     "The incredible tale and fantastical evolution of Bella Baxter, a young woman brought back to life by an unorthodox scientist."},
    {3, "The Grand Budapest Hotel", "Comedy · Drama", "8.5", "1h 39m", "grand-budapest",
     "A legendary concierge and his young protégé become caught up in the theft of a priceless painting."},
    {4, "Spider-Man: Across the Spider-Verse", "Animation · Action", "8.7", "2h 20m", "spider-verse",
     "Miles Morales travels across the multiverse and meets a team of Spider-People charged with protecting its existence."},
    {5, "Past Lives", "Drama · Romance", "8.0", "1h 45m", "past-lives",
     "Two deeply connected childhood friends are wrested apart after one family emigrates from South Korea."},
    {6, "Oppenheimer", "Biography · Drama", "8.9", "3h 00m", "oppenheimer",
     "The story of J. Robert Oppenheimer and his role in the development of the atomic bomb."}
}};

unsigned long next_booking_id = 1042;

std::string escape_json(const std::string& value) {
    std::string result;
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') {
            result.push_back('\\');
            result.push_back(static_cast<char>(ch));
        } else if (ch < 0x20) {
            result += ' ';
        } else {
            result.push_back(static_cast<char>(ch));
        }
    }
    return result;
}

std::string json_string(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const auto key_pos = body.find(marker);
    if (key_pos == std::string::npos) return {};
    const auto colon = body.find(':', key_pos + marker.size());
    const auto first_quote = body.find('"', colon == std::string::npos ? key_pos : colon + 1);
    if (first_quote == std::string::npos) return {};
    std::string value;
    bool escaped = false;
    for (std::size_t i = first_quote + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            return value;
        } else {
            value.push_back(ch);
        }
    }
    return {};
}

std::vector<std::string> json_string_array(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const auto key_pos = body.find(marker);
    if (key_pos == std::string::npos) return {};
    const auto start = body.find('[', key_pos + marker.size());
    const auto end = body.find(']', start == std::string::npos ? key_pos : start + 1);
    if (start == std::string::npos || end == std::string::npos) return {};
    std::vector<std::string> values;
    for (auto i = start + 1; i < end;) {
        i = body.find('"', i);
        if (i == std::string::npos || i >= end) break;
        const auto close = body.find('"', i + 1);
        if (close == std::string::npos || close >= end) break;
        values.push_back(body.substr(i + 1, close - i - 1));
        i = close + 1;
    }
    return values;
}

bool is_valid_date(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(date[i]))) return false;
    }
    return true;
}

std::string movies_json() {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < movies.size(); ++i) {
        const auto& movie = movies[i];
        if (i) out << ",";
        out << "{\"id\":" << movie.id
            << ",\"title\":\"" << escape_json(movie.title)
            << "\",\"genre\":\"" << escape_json(movie.genre)
            << "\",\"rating\":\"" << movie.rating
            << "\",\"duration\":\"" << movie.duration
            << "\",\"poster\":\"" << movie.poster
            << "\",\"description\":\"" << escape_json(movie.description) << "\"}";
    }
    out << "]";
    return out.str();
}

std::string response(int status, const std::string& status_text, const std::string& content_type,
                     const std::string& body) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Connection: close\r\n\r\n" << body;
    return out.str();
}

std::string mime_type(const std::string& path) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript; charset=utf-8";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
    return "application/octet-stream";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string route_request(const std::string& method, const std::string& path, const std::string& body) {
    if (method == "OPTIONS") return response(204, "No Content", "application/json", "");
    if (method == "GET" && path == "/api/movies") {
        return response(200, "OK", "application/json; charset=utf-8", movies_json());
    }
    if (method == "POST" && path == "/api/bookings") {
        const auto movie_id_text = json_string(body, "movieId");
        const auto date = json_string(body, "date");
        const auto showtime = json_string(body, "showtime");
        const auto email = json_string(body, "email");
        const auto name = json_string(body, "name");
        const auto seats = json_string_array(body, "seats");
        const auto movie = std::find_if(movies.begin(), movies.end(), [&](const Movie& item) {
            return movie_id_text == std::to_string(item.id);
        });
        const std::array<std::string, 5> showtimes{{"10:30 AM", "1:45 PM", "4:30 PM", "7:15 PM", "10:00 PM"}};
        const bool showtime_exists = std::find(showtimes.begin(), showtimes.end(), showtime) != showtimes.end();
        if (movie == movies.end() || !is_valid_date(date) || !showtime_exists ||
            seats.empty() || seats.size() > 8 || name.empty() || name.size() > 100 ||
            email.empty() || email.size() > 254 || email.find('@') == std::string::npos) {
            return response(400, "Bad Request", "application/json; charset=utf-8",
                            "{\"error\":\"Please provide valid booking and contact details (up to 8 seats).\"}");
        }
        const auto booking_id = next_booking_id++;
        std::ostringstream result;
        result << "{\"bookingId\":\"CB" << booking_id << "\",\"movie\":\"" << escape_json(movie->title)
               << "\",\"date\":\"" << escape_json(date) << "\",\"showtime\":\"" << escape_json(showtime)
               << "\",\"seats\":[";
        for (std::size_t i = 0; i < seats.size(); ++i) {
            if (i) result << ",";
            result << "\"" << escape_json(seats[i]) << "\"";
        }
        result << "],\"total\":" << seats.size() * 14 << "}";
        return response(201, "Created", "application/json; charset=utf-8", result.str());
    }
    if (method != "GET") {
        return response(405, "Method Not Allowed", "application/json; charset=utf-8",
                        "{\"error\":\"Method not allowed.\"}");
    }

    const std::string relative_path = path == "/" ? "/index.html" : path;
    if (relative_path != "/index.html" && relative_path != "/styles.css" && relative_path != "/app.js") {
        return response(404, "Not Found", "text/plain; charset=utf-8", "Not found");
    }
    const std::string file = read_file(std::string(CINEBOOK_WEB_DIR) + relative_path);
    if (file.empty()) {
        return response(500, "Internal Server Error", "text/plain; charset=utf-8",
                        "Unable to load website files.");
    }
    return response(200, "OK", mime_type(relative_path), file);
}

void handle_client(Socket client) {
    std::string request;
    std::array<char, 4096> buffer{};
    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0;
    while (request.size() < 1024 * 1024) {
#ifdef _WIN32
        const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const auto received = recv(client, buffer.data(), buffer.size(), 0);
#endif
        if (received <= 0) break;
        request.append(buffer.data(), static_cast<std::size_t>(received));
        header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            const auto length_pos = request.find("Content-Length:");
            if (length_pos != std::string::npos && length_pos < header_end) {
                const auto value_start = request.find_first_not_of(' ', length_pos + 15);
                const auto value_end = request.find("\r\n", value_start);
                if (value_start != std::string::npos) {
                    content_length = static_cast<std::size_t>(
                        std::strtoul(request.substr(value_start, value_end - value_start).c_str(), nullptr, 10));
                }
            }
            if (request.size() >= header_end + 4 + content_length) break;
        }
    }

    std::string outgoing;
    if (header_end == std::string::npos) {
        outgoing = response(400, "Bad Request", "text/plain; charset=utf-8", "Bad request");
    } else {
        const auto line_end = request.find("\r\n");
        std::istringstream request_line(request.substr(0, line_end));
        std::string method, path, version;
        request_line >> method >> path >> version;
        const auto query = path.find('?');
        if (query != std::string::npos) path.erase(query);
        const auto body_start = header_end + 4;
        const std::string body = request.substr(body_start, std::min(content_length, request.size() - body_start));
        outgoing = route_request(method, path, body);
    }

    std::size_t sent_total = 0;
    while (sent_total < outgoing.size()) {
#ifdef _WIN32
        const int sent = send(client, outgoing.data() + sent_total,
                              static_cast<int>(outgoing.size() - sent_total), 0);
#else
        const auto sent = send(client, outgoing.data() + sent_total, outgoing.size() - sent_total, 0);
#endif
        if (sent <= 0) break;
        sent_total += static_cast<std::size_t>(sent);
    }
    close_socket(client);
}
}  // namespace

int main(int argc, char* argv[]) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    if (port < 1 || port > 65535) {
        std::cerr << "Port must be between 1 and 65535.\n";
        return 1;
    }
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Unable to initialize Winsock.\n";
        return 1;
    }
#endif
    const Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == invalid_socket) {
        std::cerr << "Unable to create server socket.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(server, 16) != 0) {
        std::cerr << "Unable to bind to 127.0.0.1:" << port << ". Is the port already in use?\n";
        close_socket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    std::cout << "CineBook is running at http://127.0.0.1:" << port << "\n";
    while (true) {
        sockaddr_in client_address{};
#ifdef _WIN32
        int client_length = sizeof(client_address);
#else
        socklen_t client_length = sizeof(client_address);
#endif
        const Socket client = accept(server, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client == invalid_socket) continue;
        handle_client(client);
    }
}
