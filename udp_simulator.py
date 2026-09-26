import socket
import time
import csv

# --- CONFIGURAZIONE AUTOMATICA ---
UDP_PORT = 8080
CSV_FILE = "test.csv"
FREQ_HZ = 100.0

def get_broadcast_address():
    """Rileva automaticamente l'IP locale e calcola il broadcast della rete del router."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        
        # Converte l'IP locale (es. 192.168.1.X) nell'indirizzo di broadcast (es. 192.168.1.255)
        ip_parts = local_ip.split('.')
        ip_parts[-1] = '255'
        broadcast_ip = '.'.join(ip_parts)
        return broadcast_ip, local_ip
    except Exception:
        return "255.255.255.255", "127.0.0.1"

UDP_IP, local_ip = get_broadcast_address()
print(f"IP locale del PC rilevato: {local_ip}")
print(f"Indirizzo di Broadcast della rete impostato su: {UDP_IP}:{UDP_PORT}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Abilita i permessi di trasmissione broadcast sulla rete del router
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

print("Avvio simulazione UDP di rete...")

try:
    with open(CSV_FILE, 'r') as file:
        reader = csv.reader(file)
        
        for row in reader:
            if len(row) < 8:
                continue
                
            payload = ""
            
            # Se la colonna 1 (ax) è vuota, è un pacchetto GNSS
            if row[1] == "":
                if len(row) >= 17:
                    t    = row[0]
                    lat  = row[8]
                    lon  = row[9]
                    vel  = row[10]
                    alt  = row[11]
                    head = row[12]
                    hAcc = row[13]
                    sAcc = row[14]
                    sat  = row[15]
                    fix  = row[16]
                    
                    payload = f"[UDP RAW] GNSS, time:{t}, lat:{lat}, lon:{lon}, vel:{vel}, alt:{alt}, head:{head}, hAcc:{hAcc}, sAcc:{sAcc}, sat:{sat}, fix:{fix}"
            
            # Altrimenti, è un pacchetto IMU
            else:
                t    = row[0]
                ax   = row[1]
                ay   = row[2]
                az   = row[3]
                gx   = row[4]
                gy   = row[5]
                gz   = row[6]
                temp = row[7]
                
                payload = f"[UDP RAW] IMU, time:{t}, ax:{ax}, ay:{ay}, az:{az}, gx:{gx}, gy:{gy}, gz:{gz}, temp:{temp}"

            # Invia contemporaneamente al broadcast di rete e al broadcast universale
            if payload:
                data = payload.encode('utf-8')
                sock.sendto(data, (UDP_IP, UDP_PORT))
                sock.sendto(data, ("255.255.255.255", UDP_PORT))
            
            time.sleep(1.0 / FREQ_HZ)

except FileNotFoundError:
    print(f"ERRORE: File '{CSV_FILE}' non trovato. Assicurati che sia nella stessa cartella.")
except KeyboardInterrupt:
    print("\nSimulazione interrotta manualmente.")
finally:
    sock.close()
    print("Socket chiuso.")