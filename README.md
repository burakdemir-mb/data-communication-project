# Veri İletişimi - Hata Tespit Yöntemleri Projesi

Bu proje, veri iletişiminde kullanılan hata tespit yöntemlerini (Parity, 2D Parity, CRC, Hamming Code, Checksum) pratik olarak gösteren bir socket programlama uygulamasıdır.

## Proje Yapısı

Proje üç ana bileşenden oluşmaktadır:

1. **Client 1 (client1.c)**: Veri Gönderici
2. **Server (server.c)**: Ara Node + Veri Bozucu
3. **Client 2 (client2.c)**: Alıcı + Hata Kontrolcü

## Sistem Gereksinimleri

- GCC derleyici
- Linux/Unix sistemler için: Standart C kütüphaneleri
- Windows sistemler için: Winsock2 kütüphanesi (otomatik bağlanır)

## Derleme

### Linux/Unix/MacOS:
```bash
gcc -o client1 client1.c
gcc -o client2 client2.c
gcc -o server server.c
```

### Windows:
```bash
gcc -o client1.exe client1.c -lws2_32
gcc -o client2.exe client2.c -lws2_32
gcc -o server.exe server.c -lws2_32
```

## Kullanım

### 1. Server'ı Başlatma

```bash
./server <port>
```

Örnek:
```bash
./server 8080
```

Server belirtilen portta dinlemeye başlar ve önce client1'in, sonra client2'nin bağlanmasını bekler.

### 2. Client 1 (Veri Gönderici)

```bash
./client1 <server_ip> <port>
```

Örnek:
```bash
./client1 127.0.0.1 8080
```

Client1 çalıştırıldığında:
1. Kullanıcıdan bir satır metin girmesi istenir
2. Hata tespit yöntemi seçilir:
   - `1` = PARITY
   - `2` = 2DPAR
   - `3` = CRC16
   - `4` = HAMMING
   - `5` = CHECKSUM
3. Seçilen yönteme göre kontrol bilgisi üretilir
4. `DATA|METHOD|CONTROL` formatında server'a gönderilir

**Örnek Paket Formatı:**
```
HELLO|CRC16|87AF
```

### 3. Client 2 (Alıcı + Hata Kontrolcü)

```bash
./client2 <server_ip> <port>
```

Örnek:
```bash
./client2 127.0.0.1 8080
```

Client2 çalıştırıldığında:
1. Server'dan paketi alır
2. Paketi parse eder (data, method, incoming_control)
3. Alınan veriye göre kontrol bilgisini yeniden hesaplar
4. Gelen kontrol bilgisi ile hesaplanan kontrol bilgisini karşılaştırır
5. Sonucu ekrana yazdırır

**Örnek Çıktı:**
```
Received Data : HEZLO
Method : CRC16
Sent Check Bits : 87AF
Computed Check Bits : 92B1
Status : DATA CORRUPTED
```

## Hata Tespit Yöntemleri

### 1. Parity Bit (Parite)
- Her byte için 1'lerin sayısına göre çift/tek parite biti üretilir
- Her byte için bir parite biti hesaplanır ve hex formatında gösterilir

### 2. 2D Parity (İki Boyutlu Parite)
- Veri 8 byte'lık satırlara bölünür
- Her satır için bir parite byte'ı hesaplanır
- Tüm satır pariteleri için genel bir parite byte'ı eklenir

### 3. CRC16 (Cyclic Redundancy Check)
- CRC-CCITT (polinom: 0x1021) algoritması kullanılır
- 16-bit kontrol bilgisi üretilir
- Hex formatında gösterilir (örn: 87AF)

### 4. Hamming Code (7,4)
- Her 4-bit nibble için Hamming (7,4) kodlaması uygulanır
- Her byte'ın yüksek ve düşük nibble'ları ayrı ayrı kodlanır
- Sonuç hex formatında gösterilir

### 5. Internet Checksum (IP Checksum)
- 16-bit ones complement checksum algoritması kullanılır
- IP protokolünde kullanılan standart checksum yöntemi
- Hex formatında gösterilir

## Server Hata Enjeksiyon Yöntemleri

Server, client1'den gelen veriyi bozmak için aşağıdaki yöntemlerden bir veya daha fazlasını rastgele uygular:

1. **Bit Flip**: Verideki rastgele bir bit ters çevrilir (1→0 veya 0→1)
2. **Character Substitution**: Rastgele bir karakter başka bir karakterle değiştirilir
3. **Character Deletion**: Rastgele bir karakter silinir
4. **Character Insertion**: Veriye rastgele bir karakter eklenir
5. **Character Swapping**: İki bitişik karakter yer değiştirilir
6. **Multiple Bit Flips**: Birden fazla bit ters çevrilir (1-4 arası)
7. **Burst Error**: 3-8 karakterlik ardışık bir bölge bozulur

**Not:** Server sadece DATA kısmını bozar, METHOD ve CONTROL bilgilerini değiştirmez.

## Çalıştırma Sırası

1. **Server'ı başlatın:**
   ```bash
   ./server 8080
   ```

2. **Client1'i çalıştırın** (ayrı bir terminal):
   ```bash
   ./client1 127.0.0.1 8080
   ```
   - Metin girin (örn: HELLO)
   - Yöntem seçin (1-5 arası)

3. **Client2'yi çalıştırın** (ayrı bir terminal):
   ```bash
   ./client2 127.0.0.1 8080
   ```
   - Server'dan bozulmuş paketi alır
   - Kontrol bilgisini hesaplar ve karşılaştırır
   - Sonucu gösterir

## Örnek Senaryo

```
Terminal 1 (Server):
$ ./server 8080
Server listening on port 8080
Waiting for client1 (sender) to connect...
client1 connected
Received packet from client1: HELLO|CRC16|87AF
Original Data: HELLO
Method: CRC16
Control: 87AF
Corrupted Data: HEZLO
Waiting for client2 (receiver) to connect...
client2 connected
Forwarded corrupted packet to client2: HEZLO|CRC16|87AF

Terminal 2 (Client1):
$ ./client1 127.0.0.1 8080
Enter text (single line): HELLO
Choose method: 1=PARITY 2=2DPAR 3=CRC16 4=HAMMING 5=CHECKSUM
3
Sent packet (18 bytes): HELLO|CRC16|87AF

Terminal 3 (Client2):
$ ./client2 127.0.0.1 8080
Received Data : HEZLO
Method : CRC16
Sent Check Bits : 87AF
Computed Check Bits : 92B1
Status : DATA CORRUPTED
```

## Teknik Detaylar

- **Protokol**: TCP/IP
- **Paket Formatı**: `DATA|METHOD|CONTROL`
- **Maksimum Veri Uzunluğu**: 4096 byte (client1), 8192 byte (client2, server)
- **Port**: Kullanıcı tarafından belirlenir
- **Platform Desteği**: Linux, Unix, macOS, Windows

## Notlar

- Server sıralı çalışır: önce client1, sonra client2 bağlanmalıdır
- Server her çalıştırmada farklı hata enjeksiyonları uygular (rastgele)
- Kontrol bilgileri hex formatında gösterilir
- Windows'ta derleme için Winsock2 kütüphanesi otomatik olarak bağlanır

## Lisans

Bu proje eğitim amaçlıdır.

