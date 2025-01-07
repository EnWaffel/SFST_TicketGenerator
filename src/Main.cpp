#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <zint.h>
#include "C:\dev\SFST_TicketGenerator\lib\argparse\argparse.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "C:\dev\SFST_TicketGenerator\include\stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "C:\dev\SFST_TicketGenerator\include\stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "C:\dev\SFST_TicketGenerator\include\stb_image_resize2.h"

#define PAGE_WIDTH 2480
#define PAGE_HEIGHT 3508
#define PAGE_COLOR_CHANNELS 3
#define BARCODE_SCALE 2
#define BARCODES_PER_PAGE 64

static int ticketY = 0;
static float barcode_scale_ticket = 0.8f;
static int tickets_per_row = 3;
static int ticket_rows_per_page = 2;
static bool debug = false;
static bool drawSeparatingLine = false;

static const char* const usages[] = {
    "SFST_TicketGenerator [options] [[--] args]",
    "SFST_TicketGenerator [options]",
    NULL,
};

static std::vector<int> ids;

bool DeleteFolderAndContents(const std::wstring& folderPath) {
    std::wstring searchPath = folderPath + L"\\*";

    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open directory: " << folderPath << std::endl;
        return false;
    }

    do {
        std::wstring fileOrFolderName = findFileData.cFileName;

        if (fileOrFolderName != L"." && fileOrFolderName != L"..") {
            std::wstring fullPath = folderPath + L"\\" + fileOrFolderName;

            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!DeleteFolderAndContents(fullPath)) {
                    FindClose(hFind);
                    return false;
                }
                if (!RemoveDirectory(fullPath.c_str())) {
                    std::wcerr << L"Failed to remove directory: " << fullPath << std::endl;
                    FindClose(hFind);
                    return false;
                }
            }
            else {
                if (!DeleteFile(fullPath.c_str())) {
                    std::wcerr << L"Failed to delete file: " << fullPath << std::endl;
                    FindClose(hFind);
                    return false;
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);

    if (!RemoveDirectory(folderPath.c_str())) {
        std::wcerr << L"Failed to remove directory: " << folderPath << std::endl;
        return false;
    }

    return true;
}


std::vector<int> GenerateIds(int num, bool random)
{
    std::vector<int> ids;

    if (random)
    {
        std::srand(time(NULL));
    }

    for (int i = 0; i < num; i++)
    {
        if (random)
        {
            int n;

            do
            {
                n = std::rand() * std::rand();
            } while (std::find(ids.begin(), ids.end(), n) != ids.end());
            
            ids.push_back(n);
        }
        else
        {
            ids.push_back(i + 1);
        }
    }

    return ids;
}

void GenerateSQL(std::vector<int>& ids)
{
    HANDLE file = CreateFile(L"insert.sql", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        std::cout << "ERROR: Konnte 'insert.sql' nicht erstellen!" << std::endl;
        return;
    }

    for (int num : ids)
    {
        std::string str = "INSERT INTO papi_sfst_ticket.tickets (`id`, `used`) VALUES (";
        str.append(std::to_string(num));
        str.append(", false");
        str.append(");\n");

        WriteFile(file, str.c_str(), str.length(), NULL, NULL);
    }

    CloseHandle(file);
}

void GenerateBarcodes(int num, bool random)
{
    DeleteFile(L"insert.sql");
    DeleteFolderAndContents(L"temp\\");
    DeleteFolderAndContents(L"out\\");

    CreateDirectory(L"temp", NULL);
    CreateDirectory(L"out", NULL);

    ids = GenerateIds(num, random);
    GenerateSQL(ids);

    for (int i = 0; i < num; i++)
    {
        struct zint_symbol* symbol;
        symbol = ZBarcode_Create();

        std::string outfile = "temp/" + std::to_string(ids[i]) + ".png";
        std::cout << "Generiere... (" << i + 1 << " / " << ids.size() << ") [" << outfile << "]" << std::endl;

        memcpy_s(symbol->outfile, sizeof(symbol->outfile), outfile.c_str(), outfile.length());
        ZBarcode_Encode(symbol, (const unsigned char*)std::to_string(ids[i]).c_str(), 0);
        ZBarcode_Print(symbol, 0);

        ZBarcode_Delete(symbol);
    }
}

void CombineIntoPrintableImages()
{
    std::cout << "Generiere Bilder zum drucken..." << std::endl;

    int index = 0;
    for (int _i = 0; _i < ids.size(); _i++)
    {
        int pageSize = (2480 * 3508) * PAGE_COLOR_CHANNELS;
        stbi_uc* page = new stbi_uc[pageSize];
        memset(page, 255, pageSize);

        int pageOffsetX = 7 * BARCODE_SCALE;
        int pageOffsetY = 15 * BARCODE_SCALE;

        int barcodesIndex = 0;
        for (int i = 0; i < BARCODES_PER_PAGE; i++)
        {
            if (index >= ids.size()) break;
            int id = ids[index];

            std::cout << "Kombiniere: temp/" << id << ".png" << std::endl;

            int width = 0;
            int height = 0;
            int comp = 0;
            stbi_uc* data = stbi_load(("temp/" + std::to_string(id) + ".png").c_str(), &width, &height, &comp, PAGE_COLOR_CHANNELS);

            if (debug) std::cout << "(w: " << width << " | h: " << height << " | c: " << comp << ")" << std::endl;

            int newWidth = width * BARCODE_SCALE;
            int newHeight = height * BARCODE_SCALE;
            stbi_uc* resizedImage = new stbi_uc[newWidth * newHeight * PAGE_COLOR_CHANNELS];

            stbir_resize_uint8_linear(data, width, height, width * PAGE_COLOR_CHANNELS, resizedImage, newWidth, newHeight, newWidth * PAGE_COLOR_CHANNELS, STBIR_RGB);

            stbi_image_free(data);
            
            for (int y = 0; y < newHeight; y++)
            {
                size_t offsetDest = ((y + pageOffsetY) * PAGE_WIDTH + pageOffsetX) * PAGE_COLOR_CHANNELS;
                size_t offsetSource = y * newWidth * PAGE_COLOR_CHANNELS;

                memcpy_s(&page[offsetDest], pageSize, &resizedImage[offsetSource], newWidth * PAGE_COLOR_CHANNELS);
            }

            pageOffsetX += newWidth + 15 * BARCODE_SCALE;

            barcodesIndex++;
            if (barcodesIndex >= 5)
            {
                barcodesIndex = 0;
                pageOffsetY += newHeight + 15 * BARCODE_SCALE;
                pageOffsetX = 7 * BARCODE_SCALE;
            }

            stbi_image_free(resizedImage);
            index++;
        }


        stbi_write_png(("out/" + std::to_string(_i + 1) + ".png").c_str(), PAGE_WIDTH, PAGE_HEIGHT, PAGE_COLOR_CHANNELS, page, PAGE_WIDTH * PAGE_COLOR_CHANNELS);
        delete[] page;

        std::cout << "out/" + std::to_string(_i + 1) + ".png" << " generiert" << std::endl;

        if (index >= ids.size()) break;
    }
}

void CombineIntoPrintableImagesForTickets()
{
    std::ifstream s("ticket.cfg");
    if (s.is_open())
    {
        s >> ticketY;
        s.close();
    }

    std::cout << "Generiere Bilder zum drucken (1)..." << std::endl;

    int templateW, templateH, templateComps, templateSize;
    stbi_uc* templateData = stbi_load("ticket.png", &templateW, &templateH, &templateComps, 3);
    templateSize = templateW * templateH * templateComps;

    if (debug) std::cout << "template (w: " << templateW << " | h: " << templateH << " | c: " << templateComps << ")" << std::endl;

    for (int i = 0; i < ids.size(); i++)
    {
        int id = ids[i];

        std::cout << "Kombiniere: temp/" << id << ".png" << std::endl;

        stbi_uc* page = new stbi_uc[templateSize];
        memcpy_s(page, templateSize, templateData, templateSize);

        int width = 0;
        int height = 0;
        int comp = 0;
        stbi_uc* data = stbi_load(("temp/" + std::to_string(id) + ".png").c_str(), &width, &height, &comp, PAGE_COLOR_CHANNELS);
        if (debug) std::cout << "(w: " << width << " | h: " << height << " | c: " << comp << ")" << std::endl;

        int pageOffsetX = templateW / 2 - (width * barcode_scale_ticket) / 2;
        int pageOffsetY = ticketY;
        if (pageOffsetY + height >= templateH)
        {
            std::cout << "ERROR: Y-Offset geht ueber das Ticket Bild hinaus!" << std::endl;
            stbi_image_free(data);
            delete[] page;
            continue;
        }

        int newWidth = width * barcode_scale_ticket;
        int newHeight = height * barcode_scale_ticket;
        stbi_uc* resizedImage = new stbi_uc[newWidth * newHeight * PAGE_COLOR_CHANNELS];

        stbir_resize_uint8_linear(data, width, height, width * PAGE_COLOR_CHANNELS, resizedImage, newWidth, newHeight, newWidth * PAGE_COLOR_CHANNELS, STBIR_RGB);

        stbi_image_free(data);

        for (int y = 0; y < newHeight; y++)
        {
            size_t offsetDest = ((y + pageOffsetY) * templateW + pageOffsetX) * PAGE_COLOR_CHANNELS;
            size_t offsetSource = y * newWidth * PAGE_COLOR_CHANNELS;

            memcpy_s(&page[offsetDest], templateSize, &resizedImage[offsetSource], newWidth * PAGE_COLOR_CHANNELS);
        }

        stbi_image_free(resizedImage);

        stbi_write_png(("out/" + std::to_string(i + 1) + ".png").c_str(), templateW, templateH, PAGE_COLOR_CHANNELS, page, templateW * PAGE_COLOR_CHANNELS);
        delete[] page;

        std::cout << "out/" + std::to_string(i + 1) + ".png" << " generiert" << std::endl;;
    }

    stbi_image_free(templateData);
}

void CombinetTicketsIntoPages()
{
    std::cout << "Kombiniere Tickets..." << std::endl;

    int pageNum = 1;
    int ticketNum = 1;
    for (int i = 0; i < ids.size(); i += tickets_per_row * ticket_rows_per_page)
    {
        int pageSize = (2480 * 3508) * PAGE_COLOR_CHANNELS;
        stbi_uc* page = new stbi_uc[pageSize];
        memset(page, 255, pageSize);

        int pageOffsetX = 0;
        int pageOffsetY = 0;
        int commonHeight = 0;

        for (int _i = 0; _i < ticket_rows_per_page; _i++)
        {
            for (int j = 0; j < tickets_per_row; j++)
            {
                int width = 0;
                int height = 0;
                int comp = 0;
                stbi_uc* data = stbi_load(("out/" + std::to_string(ticketNum) + ".png").c_str(), &width, &height, &comp, PAGE_COLOR_CHANNELS);
                if (data == nullptr) break;

                DeleteFile((L"out/" + std::to_wstring(ticketNum) + L".png").c_str());
                if (debug) std::cout << "(w: " << width << " | h: " << height << " | c: " << comp << ")" << std::endl;

                for (int y = 0; y < height; y++)
                {
                    size_t offsetDest = ((y + pageOffsetY) * PAGE_WIDTH + pageOffsetX) * PAGE_COLOR_CHANNELS;
                    size_t offsetSource = y * width * PAGE_COLOR_CHANNELS;

                    memcpy_s(&page[offsetDest], PAGE_WIDTH, &data[offsetSource], width * PAGE_COLOR_CHANNELS);
                }

                stbi_image_free(data);

                pageOffsetX += width + 15;
                commonHeight += height;
                ticketNum++;
            }

            if (drawSeparatingLine)
            {
                if (pageOffsetY < PAGE_HEIGHT)
                {
                    memset(&page[(pageOffsetY * PAGE_WIDTH) * PAGE_COLOR_CHANNELS], 0, PAGE_WIDTH * PAGE_COLOR_CHANNELS);
                }
            }

            int avgHeight = commonHeight / tickets_per_row;
            pageOffsetY += commonHeight / tickets_per_row + 15;
            pageOffsetX = 0;
        }

        if (drawSeparatingLine)
        {
            if (pageOffsetY < PAGE_HEIGHT)
            {
                memset(&page[(pageOffsetY * PAGE_WIDTH) * PAGE_COLOR_CHANNELS], 0, PAGE_WIDTH * PAGE_COLOR_CHANNELS);
            }
        }

        stbi_write_png(("out/" + std::to_string(pageNum) + ".png").c_str(), PAGE_WIDTH, PAGE_HEIGHT, PAGE_COLOR_CHANNELS, page, PAGE_WIDTH * PAGE_COLOR_CHANNELS);
        delete[] page;

        std::cout << "out/" + std::to_string(pageNum) + ".png" << " generiert" << std::endl;

        pageNum++;
    }
}

static void GenerateBarcode(const char* text)
{
    DeleteFolderAndContents(L"out\\");
    CreateDirectory(L"out", NULL);

    struct zint_symbol* symbol;
    symbol = ZBarcode_Create();

    const char* outFile = "out/1.png";
    memcpy_s(symbol->outfile, sizeof(symbol->outfile), outFile, strlen(outFile));
    ZBarcode_Encode(symbol, (const unsigned char*)text, 0);
    ZBarcode_Print(symbol, 0);

    ZBarcode_Delete(symbol);

    std::cout << "Barcode generiert!" << std::endl;
}

int main(int argc, const char** argv)
{
    int ticketNum = 0;
    int _useRandomNumbers = 1;
    int _directTickets = 0;
    int _combineDirectTickets = 0;
    const char* textForBarcode = nullptr;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Barcodes fuer Tickets Generieren"),
        OPT_INTEGER('t', "tnum", &ticketNum, "Anzahl der Tickets", NULL, 0, 0),
        OPT_INTEGER('r', "rnd", &_useRandomNumbers, "Zufaellige Zahlen fuer die Tickets benutzen", NULL, 0, 0),
        OPT_BOOLEAN('d', "dt", &_directTickets, "Direkt die Barcodes auf Tickets packen", NULL, 0, 0),
        OPT_BOOLEAN('c', "cdt", &_combineDirectTickets, "Nach der Generierung die Tickets auf DIN-A4 Blaetter packen", NULL, 0, 0),
        OPT_INTEGER('p', "tp", &tickets_per_row, "Tickets per Reihe", NULL, 0, 0),
        OPT_INTEGER('v', "tr", &ticket_rows_per_page, "Reihen per Seite", NULL, 0, 0),
        OPT_FLOAT('s', "ts", &barcode_scale_ticket, "Barcode groesse auf Tickets", NULL, 0, 0),
        OPT_BOOLEAN('b', "db", &debug, "Debug Info", NULL, 0, 0),
        OPT_BOOLEAN('l', "dl", &drawSeparatingLine, "Linie zum ausschneiden", NULL, 0, 0),
        OPT_STRING('g', "gn", &textForBarcode, "Generiere Barcodes", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nSFST_TicketGenerator ist ein Barcode-Generator Programm fuer Tickets\nGeneriere Barcodes mit \"SFST_TicketGenerator -t <Ticket Anzahl>\"", "\n");
    argc = argparse_parse(&argparse, argc, argv);

    bool useRandomNumbers = _useRandomNumbers;
    bool directTickets = _directTickets;
    bool combineDirectTickets = _combineDirectTickets;

    if (textForBarcode)
    {
        GenerateBarcode(textForBarcode);
        return 0;
    }
    
    if (ticketNum < 1)
    {
        std::cout << "ERROR: Die Ticket Anzahl muss mindestens 1 sein!" << std::endl;
        return -1;
    }

    if (ticketNum < 2)
    {
        std::cout << "Generiere " << ticketNum << " Ticket..." << std::endl;
    }
    else
    {
        std::cout << "Generiere " << ticketNum << " Tickets..." << std::endl;
    }

    if (useRandomNumbers)
    {
        std::cout << "(Benutze Zufaellige Zahlen fuer die Tickets)" << std::endl;
    }
    if (directTickets)
    {
        std::cout << "(Packe Barcodes direkt auf Tickets)" << std::endl;
    }

    GenerateBarcodes(ticketNum, useRandomNumbers);
    if (directTickets)
    {
        CombineIntoPrintableImagesForTickets();
    }
    else
    {
        CombineIntoPrintableImages();
    }

    if (combineDirectTickets)
    {
        CombinetTicketsIntoPages();
    }

    std::cout << "Fertig!" << std::endl;

	return 0;
}