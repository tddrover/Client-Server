import tkinter as tk
import socket
import struct
import threading
import string
from queue import Queue
from threading import Lock

# Function to filter out non-printable ASCII characters
def filter_valid_characters(text):
    allowed_chars = string.ascii_letters + string.digits + ' \t'
    filtered_text = ''.join(c for c in text if c in allowed_chars)
    #print("Filtered Text:", filtered_text)  # Debugging output
    return filtered_text

class ChangeRoomWindow:
    def __init__(self, parent, server_socket, app_instance):
        self.parent = parent
        self.server_socket = server_socket
        self.app = app_instance
        self.window = tk.Toplevel(parent)
        self.window.title("Change Room")
        
        self.room_label = tk.Label(self.window, text="Room Number:")
        self.room_label.grid(row=0, column=0, sticky="w")
        self.room_entry = tk.Entry(self.window)
        self.room_entry.grid(row=0, column=1, sticky="ew")
        
        self.submit_button = tk.Button(self.window, text="Submit", command=self.submit_room)
        self.submit_button.grid(row=1, columnspan=2, sticky="ew")

    def submit_room(self):
        room_number = self.room_entry.get()
        room_number = int(room_number)

        room_data = bytearray(3)
        room_data[0] = 2  # Type
        room_data[1:3] = room_number.to_bytes(2, byteorder='little')  # Room Number
        
        # Send room information to the server
        try:
            self.server_socket.send(room_data)  # Send room data
            self.app.clear_text() 
        except Exception as e:
            print(f"Error sending room data to server: {e}")
        
        # Close the window
        self.window.destroy()

        
class MessageInputWindow:
    def __init__(self, parent, server_socket):
        self.parent = parent
        self.server_socket = server_socket
        self.window = tk.Toplevel(parent)
        self.window.title("Message Input")
        
        self.sender_label = tk.Label(self.window, text="Sender:")
        self.sender_label.grid(row=0, column=0, sticky="w")
        self.sender_entry = tk.Entry(self.window)
        self.sender_entry.grid(row=0, column=1, sticky="ew")
        
        self.recipient_label = tk.Label(self.window, text="Recipient:")
        self.recipient_label.grid(row=1, column=0, sticky="w")
        self.recipient_entry = tk.Entry(self.window)
        self.recipient_entry.grid(row=1, column=1, sticky="ew")
        
        self.message_label = tk.Label(self.window, text="Message:")
        self.message_label.grid(row=2, column=0, sticky="w")
        self.message_entry = tk.Entry(self.window)
        self.message_entry.grid(row=2, column=1, sticky="ew")
        
        self.submit_button = tk.Button(self.window, text="Submit", command=self.submit_message)
        self.submit_button.grid(row=3, columnspan=2, sticky="ew")

    def submit_message(self):
        sender = self.sender_entry.get()
        recipient = self.recipient_entry.get()
        message = self.message_entry.get().strip()
        
        if sender and recipient and message:
            buffer = bytearray(68 + len(message))
            buffer[0] = 1  # Type
            message_length = len(message)
            buffer[1:3] = message_length.to_bytes(2, byteorder='little')  # Message Length
            buffer[3:35] = recipient.ljust(32, '\0').encode('utf-8')  # Recipient Name
            buffer[35:65] = sender.ljust(30, '\0').encode('utf-8')  # Sender Name
            buffer[65] = 0  # End of Sender Name
            buffer[66] = 0
            buffer[67:] = message.encode('utf-8')

            total_size = 68 + len(message)
            #bytes_sent = self.server_socket.send(buffer)

        # Send message to the server
        try:
            # Assuming server_socket is the socket connected to the server
            self.server_socket.send(buffer)  # Send message data
            # Add code to handle server response if needed
        except Exception as e:
            print(f"Error sending message to server: {e}")

        # Close the window or do any other necessary UI updates
        self.window.destroy()

class LootInputWindow:
    def __init__(self, parent, server_socket):
        self.parent = parent
        self.server_socket = server_socket
        self.window = tk.Toplevel(parent)
        self.window.title("Loot Target")

        self.loot_label = tk.Label(self.window, text="Target Name:")
        self.loot_label.grid(row=0, column=0, sticky="w")
        self.loot_entry = tk.Entry(self.window)
        self.loot_entry.grid(row=0, column=1, sticky="ew")

        self.submit_button = tk.Button(self.window, text="Submit", command=self.submit_loot)
        self.submit_button.grid(row=1, columnspan=2, sticky="ew")

    def submit_loot(self):
        target_name = self.loot_entry.get()
        if len(target_name) > 32:
            print("Error: Target name too long. Please enter a name up to 32 characters.")
            return

        loot_data = bytearray(33)
        loot_data[0] = 5  # Type for LOOT
        loot_data[1:1+len(target_name)] = target_name.encode('utf-8')  # Target Name

        # Send loot information to the server
        try:
            self.server_socket.send(loot_data)  # Send loot data
        except Exception as e:
            print(f"Error sending loot data to server: {e}")

        # Close the window
        self.window.destroy()

        
class CharacterInputWindow:
    def __init__(self, parent, server_socket):
        self.parent = parent
        self.server_socket = server_socket
        self.window = tk.Toplevel(parent)
        self.window.title("Character Input")
        
        self.name_label = tk.Label(self.window, text="Name:")
        self.name_label.grid(row=0, column=0, sticky="w")
        self.name_entry = tk.Entry(self.window)
        self.name_entry.grid(row=0, column=1, sticky="ew")
        
        # Add entry fields for other attributes
        self.flags_label = tk.Label(self.window, text="Flags:")
        self.flags_label.grid(row=1, column=0, sticky="w")
        self.flags_entry = tk.Entry(self.window)
        self.flags_entry.grid(row=1, column=1, sticky="ew")

        self.attack_label = tk.Label(self.window, text="Attack:")
        self.attack_label.grid(row=2, column=0, sticky="w")
        self.attack_entry = tk.Entry(self.window)
        self.attack_entry.grid(row=2, column=1, sticky="ew")

        self.defense_label = tk.Label(self.window, text="Defense:")
        self.defense_label.grid(row=3, column=0, sticky="w")
        self.defense_entry = tk.Entry(self.window)
        self.defense_entry.grid(row=3, column=1, sticky="ew")

        self.regen_label = tk.Label(self.window, text="Regen:")
        self.regen_label.grid(row=4, column=0, sticky="w")
        self.regen_entry = tk.Entry(self.window)
        self.regen_entry.grid(row=4, column=1, sticky="ew")

        self.health_label = tk.Label(self.window, text="Health:")
        self.health_label.grid(row=5, column=0, sticky="w")
        self.health_entry = tk.Entry(self.window)
        self.health_entry.grid(row=5, column=1, sticky="ew")

        self.gold_label = tk.Label(self.window, text="Gold:")
        self.gold_label.grid(row=6, column=0, sticky="w")
        self.gold_entry = tk.Entry(self.window)
        self.gold_entry.grid(row=6, column=1, sticky="ew")

        self.room_label = tk.Label(self.window, text="Room Number:")
        self.room_label.grid(row=7, column=0, sticky="w")
        self.room_entry = tk.Entry(self.window)
        self.room_entry.grid(row=7, column=1, sticky="ew")

        self.description_label = tk.Label(self.window, text="Description:")
        self.description_label.grid(row=8, column=0, sticky="w")
        self.description_entry = tk.Entry(self.window)
        self.description_entry.grid(row=8, column=1, sticky="ew")
        
        self.submit_button = tk.Button(self.window, text="Submit", command=self.submit_character)
        self.submit_button.grid(row=9, columnspan=2, sticky="ew")

    def submit_character(self):
        # Retrieve the values entered by the user
        name = self.name_entry.get()
        flags = int(self.flags_entry.get())
        attack = int(self.attack_entry.get())
        defense = int(self.defense_entry.get())
        regen = int(self.regen_entry.get())
        health = int(self.health_entry.get())
        gold = int(self.gold_entry.get())
        room = int(self.room_entry.get())
        description = self.description_entry.get()
        
        # Ensure name is no longer than 32 characters
        name = name[:32]
        
        # Prepare the data to send to the server
        character_data = bytearray(48 + len(description))
        
        # Set the first byte to 10
        character_data[0] = 10
        
        # Encode the name and insert it into the character data
        name_encoded = name.encode("utf-8")
        character_data[1:len(name_encoded) + 1] = name_encoded
        
        # Set other fields
        struct.pack_into("<B", character_data, 33, flags)
        struct.pack_into("<H", character_data, 34, attack)
        struct.pack_into("<H", character_data, 36, defense)
        struct.pack_into("<H", character_data, 38, regen)
        struct.pack_into("<h", character_data, 40, health)
        struct.pack_into("<H", character_data, 42, gold)
        struct.pack_into("<H", character_data, 44, room)
        struct.pack_into("<H", character_data, 46, len(description))
        description_encoded = description.encode("utf-8")
        character_data[48:] = description_encoded
        
        # Send character information to the server
        try:
            # Assuming server_socket is the socket connected to the server
            self.server_socket.send(character_data)  # Send character data
            # Add code to handle server response if needed
        except Exception as e:
            print(f"Error sending character data to server: {e}")
        
        # Close the window
        self.window.destroy()

class LurkReaderApp:
    def __init__(self, root):
        self.root = root
        self.root.title("TKLurkReader")
        self.root.geometry("800x600")
        self.skt = None
        self.message_queue = Queue()
        self.lock = Lock()

        self.hostname_label = tk.Label(root, text="Hostname:")
        self.hostname_label.grid(row=0, column=0, sticky="w")

        self.hostname_entry = tk.Entry(root)
        self.hostname_entry.grid(row=0, column=1, sticky="ew")

        self.port_label = tk.Label(root, text="Port:")
        self.port_label.grid(row=0, column=2, sticky="w")

        self.port_entry = tk.Entry(root)
        self.port_entry.grid(row=0, column=3, sticky="ew")

        self.connect_button = tk.Button(root, text="Connect", command=self.connect_to_server)
        self.connect_button.grid(row=0, column=4, sticky="ew")

        self.quit_button = tk.Button(root, text="Quit", command=self.leave, state=tk.DISABLED)
        self.quit_button.grid(row=0, column=5, sticky="ew")

        self.character_button = tk.Button(root, text="Character", command=self.open_character_input_window, state=tk.DISABLED)
        self.character_button.grid(row=1, column=0, sticky="ew")

        self.start_button = tk.Button(root, text="Start", command=self.start, state=tk.DISABLED)
        self.start_button.grid(row=1, column=1, sticky="ew")

        self.message_button = tk.Button(root, text="Message", command=self.open_message_input_window, state=tk.DISABLED)
        self.message_button.grid(row=1, column=2, sticky="ew")

        self.changeroom_button = tk.Button(root, text="Change Room", command = self.open_changeroom_input_window, state=tk.DISABLED)
        self.changeroom_button.grid(row=1, column=3, sticky="ew")

        self.fight_button = tk.Button(root, text="Fight", command=self.fight, state=tk.DISABLED)
        self.fight_button.grid(row=1, column=4, sticky="ew")

        self.loot_button = tk.Button(root, text="Loot", command=self.open_loot_input_window, state=tk.DISABLED)
        self.loot_button.grid(row=1, column=5, sticky="ew")

        self.output_text = tk.Text(root, wrap="word")
        self.output_text.grid(row=2, column=0, columnspan=6, sticky="nsew")
        self.output_text.config(state="disabled")

        self.output_text.tag_config("error", foreground="red")
        self.output_text.tag_config("green", foreground="green")
        self.output_text.tag_config("purple", foreground="purple")
        self.output_text.tag_config("blue", foreground="blue")
        self.output_text.tag_config("orange", foreground="orange")

        self.connect_to_server()  # Connect to the server when the app starts
        
    def add_text(self, text, tag=None, color=None):
        self.output_text.config(state="normal")
        if tag:
            self.output_text.insert("end", text, tag)
        else:
            self.output_text.insert("end", text)
        if color and tag:
            self.output_text.tag_config(tag, foreground=color)  # Set the text color based on the tag
        self.output_text.see("end")
        self.output_text.config(state="disabled")

    def clear_text(self):
        self.output_text.config(state="normal")
        self.output_text.delete("1.0", "end")
        self.output_text.config(state="disabled")
        
    def connect_to_server(self):
        self.output_text.config(state="normal")
        self.output_text.delete("1.0", "end")
        self.output_text.config(state="disabled")
        hostname = self.hostname_entry.get()
        port_str = self.port_entry.get()

        if not port_str:
            self.add_text("Please enter a port number.\n", "error")
            return

        try:
            port = int(port_str)
            if port < 0 or port > 65535:
                raise ValueError("Port number out of range")
        except ValueError:
            self.add_text("Invalid port number. Please enter a valid integer.\n", "error")
            return

        try:
            self.add_text("\n\nConnecting...\n")
            self.skt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # Create a socket
            self.skt.connect((hostname, port))  # Connect the socket
            threading.Thread(target=self.handle_connection).start()
            self.quit_button.config(state=tk.NORMAL)
            self.character_button.config(state=tk.NORMAL)
            self.start_button.config(state=tk.NORMAL)
            self.message_button.config(state=tk.NORMAL)
            self.changeroom_button.config(state=tk.NORMAL)
            self.fight_button.config(state=tk.NORMAL)
            self.loot_button.config(state=tk.NORMAL)
        except Exception as e:
            self.add_text("Invalid port number. Please enter a valid integer\n", "error")

    def receive_all(self, skt, num_bytes):
        data = b""
        while len(data) < num_bytes:
            chunk = skt.recv(num_bytes - len(data))
            #if not chunk:
                #raise RuntimeError("Socket connection broken")
            data += chunk
        return data

    def handle_connection(self):
        try:
            while True:
                # Receive message type (1 byte)
                data = self.receive_all(self.skt, 1)
                if not data:
                    raise RuntimeError("Connection closed prematurely")
                msg_type = data[0]

                if msg_type == 1:  #Message
                    # Receiving message header
                    data += self.receive_all(self.skt, 66)
                    
                    # Unpacking message header
                    message_length = struct.unpack("<H", data[1:3])[0]
                    recipient_name = data[3:35].decode("utf-8", errors='replace').rstrip('\x00')
                    sender_name = data[35:65].decode("utf-8", errors='replace').rstrip('\x00')

                    narration_marker = struct.unpack("<BB", data[65:67])

                    # Check for narration marker
                    is_narration = narration_marker == (0, 1)
                    message = self.receive_all(self.skt, message_length).decode("utf-8", errors='replace').rstrip('\x00')

                    # Construct message information
                    message_info = f"\nType: 1 (MESSAGE)\n"
                    message_info += f"  Message Length: {message_length}\n"
                    message_info += f"  Recipient Name: {recipient_name}\n"
                    message_info += f"  Sender Name: {sender_name}\n"
                    message_info += f"  Message: {message}\n"

                    self.add_text(message_info, "purple")

                elif msg_type == 7:   #Error
                   # Receive error code (1 byte)
                    error_code = self.receive_all(self.skt, 1)[0]

                    # Receive error message length (2 bytes)
                    error_msg_length = struct.unpack("<H", self.receive_all(self.skt, 2))[0]

                    # Receive error message
                    error_msg = self.receive_all(self.skt, error_msg_length).decode("utf-8")

                    # Construct the message to display
                    error_info = f"\nType: 7 (ERROR)\n"
                    error_info += f"  Error Code: {error_code}\n"
                    error_info += f"  Error Message Length: {error_msg_length}\n"
                    error_info += f"  Error Message: {error_msg}\n"

                    self.add_text(error_info, "error")
                                    
                elif msg_type == 8:  # Accept
                    data += self.receive_all(self.skt, 1)  # Receive the second byte
                    accept_type = data[1]

                    if accept_type == 1:
                        self.add_text("\nAccepted message!\n", "green")
                    elif accept_type == 6:
                        self.add_text("\nAccepted start!\n", "green")
                    elif accept_type == 10:
                        self.add_text("\nAccepted character!\n", "green")
                    elif accept_type == 12:
                        self.add_text("\nAccepted leave!\n", "green")

                elif msg_type == 9: #Room
                    self.message_queue.put((msg_type, data))

    
                elif msg_type == 10:  # Character 
                    # Receive remaining data based on message type 10
                    self.message_queue.put((msg_type, data))


                elif msg_type == 11:  # GAME 
                    data += self.receive_all(self.skt, 6)
                    initial_points = struct.unpack("<H", data[1:3])[0]  # Unpack initial points (2 bytes)
                    stat_limit = struct.unpack("<H", data[3:5])[0]  # Unpack stat limit (2 bytes)
                    desc_length = struct.unpack("<H", data[5:7])[0]  # Unpack description length (2 bytes)

                    data += self.receive_all(self.skt, desc_length)
                    description = data[7:].decode("utf-8")  # Decode description

                    game_info = f"Received a game message:\n  Initial Points: {initial_points}\n  Stat Limit: {stat_limit}\n  Description: {description}\n"
                    self.add_text(game_info)

                elif msg_type == 13:  # Connection
                    # Receive the fixed-size part of the message
                    data += self.receive_all(self.skt, 36)
                    
                    # Extract fields from the received data
                    room_number = struct.unpack("<H", data[1:3])[0]
                    room_name_bytes = data[3:35]
                    room_description_length = struct.unpack("<H", data[35:37])[0]

                    # Receive the variable-length part (room description)
                    room_description_bytes = self.receive_all(self.skt, room_description_length)

                    # Handle room name decoding properly
                    room_name = self.clean_decode(room_name_bytes)
                    room_description = room_description_bytes.decode("utf-8", errors='replace').rstrip('\x00')

                    # Construct the message to display
                    room_info = f"\nType: 13 (CONNECTION)\n"
                    room_info += f"  Number: {room_number}\n"
                    room_info += f"  Name: {room_name}\n"
                    room_info += f"  Description Length: {room_description_length}\n"
                    room_info += f"  Room Description: {room_description}\n"
                    
                    self.add_text(room_info)


                elif msg_type == 14:  # VERSION 
                    data += self.receive_all(self.skt, 2)  
                    major = data[1]
                    minor = data[2]
                    version_info = f"Server version: {major}.{minor}\n"
                    self.add_text(version_info)

                self.process_messages()

        except Exception as e:
            self.add_text(f"Error: {e}\n", "error")

        finally:
            self.skt.close()
            
    def clean_decode(self, byte_data):
        # Decode using UTF-8 and ignore errors
        decoded_str = byte_data.decode('utf-8', errors='ignore')
        # Find the first occurrence of a control or non-ASCII character and cut off there
        printable_str = ''
        for char in decoded_str:
            if ord(char) < 32 or ord(char) > 126:
                break
            printable_str += char
        return printable_str.strip()

    def process_messages(self):
        while not self.message_queue.empty():
            msg_type, data = self.message_queue.get()
            if msg_type == 9:  # Room
                self.process_room(data)
            elif msg_type == 10:  # Character 
                self.process_character(data)

    def process_room(self, data):
        with self.lock:
                data += self.receive_all(self.skt, 36)
                
                room_number = struct.unpack("<H", data[1:3])[0]
                room_name_bytes = data[3:35]
                room_description_length = struct.unpack("<H", data[35:37])[0]
                room_name = self.clean_decode(room_name_bytes)
                room_description = self.receive_all(self.skt, room_description_length).decode("utf-8", errors='replace').rstrip('\x00')

                room_info = f"\nType: 9 (ROOM)\n"
                room_info += f"  Number: {room_number}\n"
                room_info += f"  Name: {room_name}\n"
                room_info += f"  Description Length: {room_description_length}\n"
                room_info += f"  Room Description: {room_description}\n"
                
                self.add_text(room_info)

    def process_character(self, data):
        with self.lock:
            # Receive remaining data based on message type 10
            data += self.receive_all(self.skt, 47)
            
            player_name_bytes = data[1:33]
            flags = data[33]
            flags_byte = data[33]
            hex_flags = hex(flags)
            attack = struct.unpack("<H", data[34:36])[0]
            defense = struct.unpack("<H", data[36:38])[0]
            regen = struct.unpack("<H", data[38:40])[0]
            health = struct.unpack("<h", data[40:42])[0]
            gold = struct.unpack("<H", data[42:44])[0]
            room = struct.unpack("<H", data[44:46])[0]
            description_length = struct.unpack("<H", data[46:48])[0]
            description = self.receive_all(self.skt, description_length).decode("utf-8").rstrip('\x00')
            player_name = self.clean_decode(player_name_bytes)
            # Construct message to display
            character_info = f"\nType: 10 (CHARACTER)\n"
            character_info += f"  Name: {player_name}\n"
            character_info += f"  Flags: {hex_flags}\n"
            character_info += f"  Attack: {attack}\n"
            character_info += f"  Defense: {defense}\n"
            character_info += f"  Regen: {regen}\n"
            character_info += f"  Health: {health}\n"
            character_info += f"  Gold: {gold}\n"
            character_info += f"  Current Room: {room}\n"
            character_info += f"  Length: {description_length}\n"
            character_info += f"  Description: {description}\n"

            if flags_byte & (1 << 5):       #If monster
                self.add_text(character_info, "orange")
            else:
                self.add_text(character_info, "blue")

    def start(self):
        try:
            self.skt.send(bytes([6]))  # Send byte with value 6 to the server
        except Exception as e:
            self.add_text(f"Error sending byte: {e}\n", "error")

    def leave(self):
        try:
            self.skt.send(bytes([12]))  # Send byte with value 12 to the server
            self.add_text("\nQuitting!!\n")
            self.quit_button.config(state=tk.DISABLED)
            self.character_button.config(state=tk.DISABLED)
            self.start_button.config(state=tk.DISABLED)
            self.message_button.config(state=tk.DISABLED)
            self.changeroom_button.config(state=tk.DISABLED)
            self.fight_button.config(state=tk.DISABLED)
            self.loot_button.config(state=tk.DISABLED)
        except Exception as e:
            self.add_text(f"Error sending byte: {e}\n", "error")

    def fight(self):
        try:
            self.skt.send(bytes([3]))  # Send byte with value 6 to the server
        except Exception as e:
            self.add_text(f"Error sending byte: {e}\n", "error")

    def open_message_input_window(self):
        message_input_window = MessageInputWindow(self.root, self.skt)

    def open_changeroom_input_window(self):
        changeroom_input_window = ChangeRoomWindow(self.root, self.skt, self)
        changeroom_input_window.window.mainloop()

    def open_character_input_window(self):
        character_input_window = CharacterInputWindow(self.root, self.skt)

    def open_loot_input_window(self):
        loot_input_window = LootInputWindow(self.root, self.skt)

        

def main():
    root = tk.Tk()
    app = LurkReaderApp(root)
    root.columnconfigure(0, weight=1)
    root.columnconfigure(1, weight=1)
    root.columnconfigure(2, weight=1)
    root.columnconfigure(3, weight=1)
    root.columnconfigure(4, weight=1)
    root.columnconfigure(5, weight=1)

    root.rowconfigure(2, weight=2)     # Row 1 (text area)

    root.mainloop()

if __name__ == "__main__":
    main()
