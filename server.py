
import socket
import argparse
import sys
import time
import select



def recv(sock):
    buf = b""
    while True:
        # Use select to check if there's data to read
        readable,_,_= select.select([sock], [], [], 1)
        if readable:
            data = sock.recv(1)
            if not data:
                break
            if data.decode("utf-8") == "\n":
                break
            buf += data
        else:
            # Timeout reached
            break
    return buf

def main(ip, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    connected = False
    max_attempts = 10
    attempts = 0

    while not connected:
        result = sock.connect_ex((ip, port))
        if result == 0:
            connected = True
            print("Connected to the socket")
        else:
            attempts += 1
            print("Connection failed. Retrying in 1 second...")
            time.sleep(1)

        if attempts == max_attempts:
            print(f"Max connection attempts reached ({max_attempts}). Exiting program.")
            sys.exit(1)

    sock.settimeout(5)  # Set the timeout to 30 seconds
    irrigation=1

    try:
        time.sleep(2)
        print("Wait until 2 seconds.")
        time_irrigation=time.time()
        
        while connected:
            current_time = time.time()
            

            try:
                #print(current_time - time_irrigation)
                
                if(current_time - time_irrigation  >=45):
                    print("sending irrigation message from server")
                    sock.send(b"4\n")
                    irrigation=0
                    time_irrigation = time.time()
                data = recv(sock)
               
                try:
                    int_value = int(data.decode("utf-8"))
                    if(int_value>50):
                        sock.send(b"1\n")
                except ValueError:
                    print("continue")
                print(data.decode("utf-8"))
                
  
                
            except socket.timeout:
                print("No more message received. Closing the socket and the program.")
                break

            #  time.sleep(1)
    except ConnectionResetError:
        print("Connection with the router closed.")
    finally:
        sock.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", dest="ip", type=str)
    parser.add_argument("--port", dest="port", type=int)
    args = parser.parse_args()

    main(args.ip, args.port)
