import random
import datetime
import time

# --- Parâmetros de Configuração ---
NUM_LINES = 1000000  # Altere aqui para o número de linhas desejado
OUTPUT_FILE = "access_log_large.txt"

# --- Dados para Geração Aleatória ---
IP_ADDRESSES = [
    "192.168.1.10", "10.0.0.5", "172.16.31.40", "203.0.113.78",
    "198.51.100.22", "8.8.8.8", "1.1.1.1", "127.0.0.1",
    "54.12.34.56", "98.76.54.32", "11.22.33.44", "66.55.44.33"
]

RESOURCES = [
    "/index.html", "/images/logo.png", "/css/style.css", "/js/main.js",
    "/about.html", "/contact.html", "/products/item1.html", "/login",
    "/admin/", "/assets/font.woff2", "/api/data", "/images/banner.jpg"
]

ERROR_RESOURCES = [
    "/old_page.html", "/images/nonexistent.gif", "/wp-admin.php",
    "/private/secret.txt", "/downloads/removed_file.zip"
]

USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.1 Safari/605.1.15",
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:89.0) Gecko/20100101 Firefox/89.0",
    "Googlebot/2.1 (+http://www.google.com/bot.html)"
]

# Pesos: 200 (OK), 404 (Not Found), 301 (Redirect), 500 (Error)
STATUS_CODES = [200, 404, 301, 500]
STATUS_WEIGHTS = [85, 10, 3, 2] # 85% de chance de ser 200, 10% de 404, etc.

# --- Geração do Arquivo ---
print(f"Gerando {NUM_LINES:,} linhas de log para o arquivo '{OUTPUT_FILE}'...")
start_time = time.time()

# Usamos uma data fixa para consistência
current_time = datetime.datetime(2025, 9, 15, 10, 0, 0)

with open(OUTPUT_FILE, "w") as f:
    for i in range(NUM_LINES):
        ip = random.choice(IP_ADDRESSES)
        
        # Incrementa o tempo realisticamente
        time_delta = datetime.timedelta(seconds=random.randint(0, 5))
        current_time += time_delta
        timestamp = current_time.strftime('[%d/%b/%Y:%H:%M:%S %z-0300]')

        method = "GET"
        
        status = random.choices(STATUS_CODES, weights=STATUS_WEIGHTS, k=1)[0]

        if status == 404:
            resource = random.choice(ERROR_RESOURCES)
            bytes_sent = random.randint(200, 500)
        else:
            resource = random.choice(RESOURCES)
            if status == 200:
                bytes_sent = random.randint(500, 50000)
            else:
                bytes_sent = random.randint(100, 300)

        user_agent = random.choice(USER_AGENTS)

        log_line = f'{ip} - - {timestamp} "{method} {resource} HTTP/1.1" {status} {bytes_sent} "{user_agent}"\n'
        f.write(log_line)

        # Barra de progresso simples
        if (i + 1) % (NUM_LINES // 100) == 0:
            progress = (i + 1) / NUM_LINES * 100
            print(f"\rProgresso: {progress:.0f}%", end="")

end_time = time.time()
print(f"\nArquivo '{OUTPUT_FILE}' gerado com sucesso em {end_time - start_time:.2f} segundos.")