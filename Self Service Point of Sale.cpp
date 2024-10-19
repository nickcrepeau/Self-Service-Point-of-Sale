// Self Service Point of Sale Script

#include "pch.h"
#include "resource.h"
#include <windows.h>
#include <commctrl.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define CONNECTION_STRING L"Driver={SQL Server};Server=localhost\\SQLEXPRESS;Database=PointOfSale;Trusted_Connection=yes;"
#define QUERY_PRODUCTS L"SELECT Name, Price, Tax, Weight FROM Products WHERE Code = ?"
#define QUERY_USERS L"SELECT Password, Name, Manager FROM Users WHERE ID = ?"
#define QUERY_SYSTEM L"SELECT TOP 1 StartingBalance, Balance, CardCharges, TaxRate FROM System"
#define WM_UPDATE_LIST (WM_USER + 1)

using namespace std;

long long g_ItemCode = 0;
string g_ItemName = "";
double g_ItemPrice = 0;
bool g_ItemTax = 0;
bool g_ItemWeight = 0;
double g_ItemQuantity = 0;
double g_Subtotal = 0;
double g_Tax = 0;
double g_Total = 0;

struct Item {
    string name;
    double price;
    double quantity;
    bool tax;
};

const int MAX_ITEMS = 13;

struct ItemLookup {
    SQLBIGINT code;
    string name;
};

// Array to store lookup items
ItemLookup lookupItems[MAX_ITEMS];
int lookupItemCount = 0;

vector<Item> itemList;

void SetListBoxFont(HWND hListBox) {
    HFONT hFont = CreateFont(
        32,                        // Height of the font
        0,                         // Default width
        0,                         // Default angle
        0,                         // Default base line
        FW_NORMAL,                 // Font weight
        FALSE,                     // Italic
        FALSE,                     // Underline
        FALSE,                     // Strikeout
        DEFAULT_CHARSET,           // Character set
        OUT_OUTLINE_PRECIS,        // Output precision
        CLIP_DEFAULT_PRECIS,       // Clipping precision
        DEFAULT_QUALITY,           // Quality
        FIXED_PITCH,               // Pitch and family
        TEXT("Consolas"));         // Font name

    SendMessage(hListBox, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void PrintDiagRec(SQLHANDLE handle, SQLSMALLINT handleType) {
    SQLWCHAR sqlState[1024];
    SQLWCHAR messageText[1024];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;

    SQLSMALLINT i = 1;
    while (SQLGetDiagRecW(handleType, handle, i, sqlState, &nativeError, messageText, sizeof(messageText) / sizeof(SQLWCHAR), &textLength) == SQL_SUCCESS) {
        std::wcout << L"SQLState: " << sqlState << std::endl;
        std::wcout << L"Message: " << messageText << std::endl;
        std::wcout << L"Native Error: " << nativeError << std::endl;
        ++i;
    }
}

INT_PTR CALLBACK PopupDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

double FetchStartingBalanceFromDatabase() {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;
    SQLDOUBLE startingBalance;
    SQLLEN startingBalanceLen;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return -1.0;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Execute the query
    retCodeSystem = SQLExecDirectW(hStmtSystem, (SQLWCHAR*)QUERY_SYSTEM, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Bind the column for Starting Balance
    retCodeSystem = SQLBindCol(hStmtSystem, 1, SQL_C_DOUBLE, &startingBalance, 0, &startingBalanceLen);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind column for Starting Balance", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Fetch the result
    if (SQLFetch(hStmtSystem) != SQL_SUCCESS) {
        MessageBox(NULL, L"Failed to fetch data", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return startingBalance;
}

double FetchBalanceFromDatabase() {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;
    SQLDOUBLE balance;
    SQLLEN balanceLen;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return -1.0;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Execute the query
    retCodeSystem = SQLExecDirectW(hStmtSystem, (SQLWCHAR*)QUERY_SYSTEM, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Bind the column for Balance
    retCodeSystem = SQLBindCol(hStmtSystem, 2, SQL_C_DOUBLE, &balance, 0, &balanceLen);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind column for Balance", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Fetch the result
    if (SQLFetch(hStmtSystem) != SQL_SUCCESS) {
        MessageBox(NULL, L"Failed to fetch data", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return balance;
}

double FetchCardChargesFromDatabase() {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;
    SQLDOUBLE cardCharges;
    SQLLEN cardChargesLen;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return -1.0;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Execute the query
    retCodeSystem = SQLExecDirectW(hStmtSystem, (SQLWCHAR*)QUERY_SYSTEM, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Bind the column for Card Charges
    retCodeSystem = SQLBindCol(hStmtSystem, 3, SQL_C_DOUBLE, &cardCharges, 0, &cardChargesLen);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind column for Card Charges", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Fetch the result
    if (SQLFetch(hStmtSystem) != SQL_SUCCESS) {
        MessageBox(NULL, L"Failed to fetch data", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return cardCharges;
}

double FetchTaxRateFromDatabase() {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;
    SQLDOUBLE taxRate;
    SQLLEN taxRateLen;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return -1.0;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Execute the query
    retCodeSystem = SQLExecDirectW(hStmtSystem, (SQLWCHAR*)QUERY_SYSTEM, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Bind the column for Tax Rate
    retCodeSystem = SQLBindCol(hStmtSystem, 4, SQL_C_DOUBLE, &taxRate, 0, &taxRateLen);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind column for Tax Rate", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Fetch the result
    if (SQLFetch(hStmtSystem) != SQL_SUCCESS) {
        MessageBox(NULL, L"Failed to fetch data", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return -1.0;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return taxRate;
}

void UpdateTotalDisplay(HWND hTotalList) {
    g_Subtotal = 0;
    g_Tax = 0;
    g_Total = 0;
    double taxRate = FetchTaxRateFromDatabase();

    // Calculate subtotal, tax, and total
    for (const auto& item : itemList) {
        g_Subtotal += item.price;
        if (item.tax) {
            g_Tax += item.price * (taxRate * 0.01); // Fetches tax rate from System database and converts to percentage
        }
    }
    g_Total = g_Subtotal + g_Tax;

    // Prepare the display string
    std::ostringstream totalString;
    SendMessageA(hTotalList, LB_RESETCONTENT, 0, 0); // Clear the list box

    totalString << "Subtotal: $" << std::fixed << std::setprecision(2) << g_Subtotal;
    SendMessageA(hTotalList, LB_ADDSTRING, 0, (LPARAM)totalString.str().c_str());
    totalString.str("");

    totalString << "Tax:      $" << std::fixed << std::setprecision(2) << g_Tax;
    SendMessageA(hTotalList, LB_ADDSTRING, 0, (LPARAM)totalString.str().c_str());
    totalString.str("");

    totalString << "Total:    $" << std::fixed << std::setprecision(2) << g_Total;
    SendMessageA(hTotalList, LB_ADDSTRING, 0, (LPARAM)totalString.str().c_str());
}

INT_PTR CALLBACK EnterWeightDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 8
        SendDlgItemMessage(hDlg, IDC_EDIT_WEIGHT, EM_SETLIMITTEXT, 8, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            char weightEntry[9]; // 8 characters + 1 for null terminator
            GetDlgItemTextA(hDlg, IDC_EDIT_WEIGHT, weightEntry, sizeof(weightEntry));

            try {
                g_ItemQuantity = std::stod(weightEntry); // Convert to double
                EndDialog(hDlg, IDOK);
            }
            catch (const std::invalid_argument&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
        }
        return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

INT_PTR CALLBACK EnterQuantityDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 8 for the quantity edit control
        SendDlgItemMessage(hDlg, IDC_EDIT_QUANTITY, EM_SETLIMITTEXT, 8, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            char quantityEntry[9]; // 8 characters + 1 for null terminator
            GetDlgItemTextA(hDlg, IDC_EDIT_QUANTITY, quantityEntry, sizeof(quantityEntry));

            try {
                g_ItemQuantity = std::stod(quantityEntry); // Convert to double
                EndDialog(hDlg, IDOK);
            }
            catch (const std::invalid_argument&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
        }
        return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

void RetrieveItemData(long long itemCode, HWND hDlg) {
    SQLHENV hEnvItem;
    SQLHDBC hDbcItem;
    SQLHSTMT hStmtItem;
    SQLRETURN retCodeItem;

    // Allocate environment handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    SQLSetEnvAttr(hEnvItem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate connection handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvItem, &hDbcItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Connect to the database
    retCodeItem = SQLDriverConnectW(hDbcItem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcItem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Allocate statement handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcItem, &hStmtItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Prepare the query
    retCodeItem = SQLPrepareW(hStmtItem, (SQLWCHAR*)QUERY_PRODUCTS, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Query preparation failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Bind the parameter for the query
    retCodeItem = SQLBindParameter(hStmtItem, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &itemCode, 0, NULL);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Execute the query
    retCodeItem = SQLExecute(hStmtItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    SQLCHAR name[21] = { 0 }; // nchar(20) + null terminator
    SQLDOUBLE price = 0.0;
    SQLLEN nameLen = 0, priceLen = 0, taxLen = 0, weightLen = 0;
    SQLINTEGER tax = 0, weight = 0;

    // Bind columns
    retCodeItem = SQLBindCol(hStmtItem, 1, SQL_C_CHAR, name, sizeof(name), &nameLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Name", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    retCodeItem = SQLBindCol(hStmtItem, 2, SQL_C_DOUBLE, &price, 0, &priceLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Price", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    retCodeItem = SQLBindCol(hStmtItem, 3, SQL_C_BIT, &tax, 0, &taxLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Tax", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    retCodeItem = SQLBindCol(hStmtItem, 4, SQL_C_BIT, &weight, 0, &weightLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Weight", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Fetch data
    bool dataFetched = false;
    while (SQLFetch(hStmtItem) == SQL_SUCCESS) {
        dataFetched = true;

        // Convert SQLCHAR to std::string
        std::string str(reinterpret_cast<char*>(name)); // Convert SQLCHAR to std::string

        // Store fetched data in global variables
        g_ItemName = str;
        g_ItemPrice = price;
        g_ItemTax = tax;
        g_ItemWeight = weight;
        g_ItemQuantity = 1; // Assuming quantity is 1

        // Determine if weight or quantity dialog should be shown
        if (g_ItemWeight) {
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_WEIGHT), hDlg, EnterWeightDlgProc) != IDOK) {
                // Cancelled or error, exit early
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
                SQLDisconnect(hDbcItem);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
                return;
            }
        }
        else {
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_QUANTITY), hDlg, EnterQuantityDlgProc) != IDOK) {
                // Cancelled or error, exit early
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
                SQLDisconnect(hDbcItem);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
                return;
            }
        }
        g_ItemPrice *= g_ItemQuantity;

        // Post a message to update the list box in the main dialog
        PostMessage(GetParent(hDlg), WM_UPDATE_LIST, 0, 0);

        // Display item details for debugging
        // std::ostringstream detailOss;
        // detailOss << "Item Name: " << str << ", Price: " << price << ", Tax: " << tax << ", Weight: " << weight;
        // MessageBoxA(NULL, detailOss.str().c_str(), "Item Details", MB_OK | MB_ICONINFORMATION);
    }

    if (!dataFetched) {
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_NOT_FOUND), hDlg, PopupDlgProc);
    }

    // Free statement handle
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);

    // Disconnect and free handles
    SQLDisconnect(hDbcItem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
}

void RetrieveItemsByFirstLetter(char firstLetter, HWND hDlg) {
    SQLHENV hEnvItem;
    SQLHDBC hDbcItem;
    SQLHSTMT hStmtItem;
    SQLRETURN retCodeItem;
    SQLWCHAR query[256];

    // Initialize lookupItemCount
    lookupItemCount = 0;

    // Allocate environment handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    SQLSetEnvAttr(hEnvItem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate connection handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvItem, &hDbcItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Connect to the database
    retCodeItem = SQLDriverConnectW(hDbcItem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcItem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Allocate statement handle
    retCodeItem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcItem, &hStmtItem);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Prepare the query to get items starting with the specified letter
    swprintf(query, sizeof(query) / sizeof(SQLWCHAR), L"SELECT Code, Name FROM Products WHERE Name LIKE N'%c%%'", firstLetter);

    retCodeItem = SQLExecDirectW(hStmtItem, query, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    SQLWCHAR name[256];
    SQLLEN nameLen = 0;
    SQLBIGINT code = 0;
    SQLLEN codeLen = 0;

    // Bind columns
    retCodeItem = SQLBindCol(hStmtItem, 1, SQL_C_SBIGINT, &code, 0, &codeLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Code", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    retCodeItem = SQLBindCol(hStmtItem, 2, SQL_C_WCHAR, name, sizeof(name) / sizeof(SQLWCHAR), &nameLen);
    if (!SQL_SUCCEEDED(retCodeItem)) {
        MessageBox(NULL, L"Failed to bind column for Name", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtItem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);
        SQLDisconnect(hDbcItem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);
        return;
    }

    // Fetch data and store in array
    while (SQLFetch(hStmtItem) == SQL_SUCCESS && lookupItemCount < MAX_ITEMS) {
        lookupItems[lookupItemCount].code = code;

        // Convert WCHAR to std::string and trim trailing spaces
        std::wstring wstr(name);
        std::string str(wstr.begin(), wstr.end());

        // Trim trailing spaces
        str.erase(str.find_last_not_of(' ') + 1);

        lookupItems[lookupItemCount].name = str;
        lookupItemCount++;
    }

    // Free statement handle
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtItem);

    // Disconnect and free handles
    SQLDisconnect(hDbcItem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcItem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvItem);

    // Sort lookupItems alphabetically by name
    std::sort(lookupItems, lookupItems + lookupItemCount, [](const ItemLookup& a, const ItemLookup& b) {
        return a.name < b.name;
    });

    // Update dialog buttons with names
    for (int i = 0; i < lookupItemCount; i++) {
        // Convert std::string to std::wstring
        std::wstring wstr(lookupItems[i].name.begin(), lookupItems[i].name.end());
        SetWindowTextW(GetDlgItem(hDlg, IDC_BUTTON_1 + i), wstr.c_str());
        ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_1 + i), SW_SHOW);
    }

    // Hide remaining buttons if fewer than 13 items
    for (int i = lookupItemCount; i < MAX_ITEMS; i++) {
        ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_1 + i), SW_HIDE);
    }
}

INT_PTR CALLBACK ItemLookupDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static char firstLetter = 'A';  // Default value

    switch (message) {
    case WM_INITDIALOG:
        // Retrieve the first letter passed as LPARAM
        firstLetter = static_cast<char>(lParam);

        // Initialize dialog with items using the first letter
        RetrieveItemsByFirstLetter(firstLetter, hDlg);
        return TRUE;

    case WM_COMMAND:
    {
        int buttonID = LOWORD(wParam);

        // Handle button clicks
        if (buttonID >= IDC_BUTTON_1 && buttonID <= IDC_BUTTON_13) {
            int index = buttonID - IDC_BUTTON_1;
            if (index >= 0 && index < lookupItemCount) {
                // Use the selected item
                RetrieveItemData(lookupItems[index].code, hDlg);
            }
            EndDialog(hDlg, buttonID);
            return TRUE;
        }
        // Handle cancel button if defined explicitly
        else if (buttonID == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
    }
    break;

    case WM_CLOSE:
        // Handle window close event (e.g., close button)
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

void ShowItemLookupDialog(HWND hParent, char firstLetter) {
    // Show the dialog to lookup items and pass firstLetter as parameter
    int result = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ITEM_LOOKUP), hParent, ItemLookupDlgProc, static_cast<LPARAM>(firstLetter));
    if (result == -1) {
        MessageBox(NULL, L"Failed to create dialog", L"Error", MB_OK | MB_ICONERROR);
    }
}

INT_PTR CALLBACK LookupDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BUTTON_A:
        case IDC_BUTTON_B:
        case IDC_BUTTON_C:
        case IDC_BUTTON_D:
        case IDC_BUTTON_E:
        case IDC_BUTTON_F:
        case IDC_BUTTON_G:
        case IDC_BUTTON_H:
        case IDC_BUTTON_I:
        case IDC_BUTTON_J:
        case IDC_BUTTON_K:
        case IDC_BUTTON_L:
        case IDC_BUTTON_M:
        case IDC_BUTTON_N:
        case IDC_BUTTON_O:
        case IDC_BUTTON_P:
        case IDC_BUTTON_Q:
        case IDC_BUTTON_R:
        case IDC_BUTTON_S:
        case IDC_BUTTON_T:
        case IDC_BUTTON_U:
        case IDC_BUTTON_V:
        case IDC_BUTTON_W:
        case IDC_BUTTON_X:
        case IDC_BUTTON_Y:
        case IDC_BUTTON_Z:
            EndDialog(hDlg, LOWORD(wParam)); // Close dialog with IDC_BUTTON_ corresponding with case selected
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL); // Close dialog with IDCANCEL
            return TRUE;
        }
        break;
    }

    return FALSE;
}

void ShowLookupDialog(HWND hParent) {
    // Show the dialog to lookup items
    int result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_LOOKUP), hParent, LookupDlgProc);

    if (result >= IDC_BUTTON_A && result <= IDC_BUTTON_Z) {
        char firstLetter = 'A' + (result - IDC_BUTTON_A); // Calculate the letter based on the button ID
        ShowItemLookupDialog(hParent, firstLetter);
    }
}

INT_PTR CALLBACK ItemEntryDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 19 for the item code edit control
        SendDlgItemMessage(hDlg, IDC_EDIT_ITEM, EM_SETLIMITTEXT, 19, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            char tempEntry[20]; // 19 characters + 1 for null terminator
            GetDlgItemTextA(hDlg, IDC_EDIT_ITEM, tempEntry, sizeof(tempEntry));

            // Check if the input is empty
            if (strlen(tempEntry) == 0) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_EMPTY), hDlg, PopupDlgProc);
                return TRUE; // Do not end the dialog
            }

            try {
                // Convert the retrieved string to a long long
                g_ItemCode = std::stoll(tempEntry);

                // Validate the g_ItemCode
                if (g_ItemCode < 0.0) {
                    throw std::out_of_range("Item Code cannot be negative.");
                }
                else if (g_ItemCode > 9223372036854775807) {
                    throw std::out_of_range("Item Code cannot be greater than 9223372036854775807.");
                }

                // Retrieve item data from the database
                RetrieveItemData(g_ItemCode, hDlg);
                EndDialog(hDlg, IDOK);
            }


            catch (const std::invalid_argument&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
            return TRUE;

        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowEnterItemDialog(HWND hParent) {
    // Show the dialog and pass the address of the g_ItemCode variable
    int result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_ITEM), hParent, ItemEntryDlgProc);

    if (result == IDOK) {
        // Convert the integer g_ItemCode to a string
        std::ostringstream oss;
        oss << g_ItemCode;
        std::string codeStr = oss.str();

        // Display the item code in a message box for debugging
        // MessageBoxA(hParent, codeStr.c_str(), "Item Code Entered", MB_OK | MB_ICONINFORMATION);
    }
    else if (result == IDCANCEL) {
        // Handle cancellation
        // MessageBoxA(hParent, "Item entry was canceled.", "Canceled", MB_OK | MB_ICONINFORMATION);
    }
}

INT_PTR CALLBACK EmployeeLoginDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    SQLHENV hEnvUsers = NULL;
    SQLHDBC hDbcUsers = NULL;
    SQLHSTMT hStmtUsers = NULL;
    SQLRETURN retCodeUsers;

    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 3 for user ID and 4 for password inputs
        SendDlgItemMessage(hDlg, IDC_EDIT_USERID, EM_SETLIMITTEXT, 3, 0);
        SendDlgItemMessage(hDlg, IDC_EDIT_PASSWORD, EM_SETLIMITTEXT, 4, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {

            // Allocate environment handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            SQLSetEnvAttr(hEnvUsers, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

            // Allocate connection handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_DBC, hEnvUsers, &hDbcUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Connect to the database
            retCodeUsers = SQLDriverConnectW(hDbcUsers, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hDbcUsers, SQL_HANDLE_DBC);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Get entered text from edit controls
            WCHAR userIDStr[5]; // 4 characters + null terminator
            WCHAR passwordStr[5]; // 4 characters + null terminator
            GetDlgItemTextW(hDlg, IDC_EDIT_USERID, userIDStr, sizeof(userIDStr) / sizeof(WCHAR));
            GetDlgItemTextW(hDlg, IDC_EDIT_PASSWORD, passwordStr, sizeof(passwordStr) / sizeof(WCHAR));

            // Convert to integers
            int userID = _wtoi(userIDStr);
            int password = _wtoi(passwordStr);

            // Allocate statement handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_STMT, hDbcUsers, &hStmtUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Prepare the query
            retCodeUsers = SQLPrepareW(hStmtUsers, (SQLWCHAR*)QUERY_USERS, SQL_NTS);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Query preparation failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Bind the parameter for the query
            retCodeUsers = SQLBindParameter(hStmtUsers, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &userID, 0, NULL);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Execute the query
            retCodeUsers = SQLExecute(hStmtUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            SQLSMALLINT fetchedPassword = 0;
            WCHAR name[21] = { 0 }; // nchar(20) + null terminator
            SQLLEN passwordLen = 0, nameLen = 0, managerLen = 0;

            // Bind columns
            retCodeUsers = SQLBindCol(hStmtUsers, 1, SQL_C_SSHORT, &fetchedPassword, 0, &passwordLen);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind column for Password", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            retCodeUsers = SQLBindCol(hStmtUsers, 2, SQL_C_WCHAR, name, sizeof(name), &nameLen);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind column for Name", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Fetch data
            int isValid = 0;
            while (SQLFetch(hStmtUsers) == SQL_SUCCESS) {
                if (passwordLen > 0 && fetchedPassword == password) {
                    isValid = 1;
                    break;
                }
            }

            // Clean up
            SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
            SQLDisconnect(hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);

            // Determine dialog result
            if (isValid == 1) {
                EndDialog(hDlg, IDOK);
            }
            else {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INCORRECT), hDlg, PopupDlgProc);
            }
        }
        return TRUE;

        case IDCANCEL:
            // Cleanup and close the dialog
            SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
            SQLDisconnect(hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL ShowEmployeeLoginDialog(HWND hParent) {
    // Show the dialog to get user ID and password
    int result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_MANAGER_LOGIN), hParent, EmployeeLoginDlgProc);

    if (result == IDOK) {
        return true;
    }
    else if (result == IDCANCEL) {
        return false;
    }
    else {
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INCORRECT), hParent, PopupDlgProc);
    }
}

bool UpdateStartingBalanceInDatabase(double newStartingBalance) {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Prepare the update query
    WCHAR updateQuery[] = L"UPDATE System SET StartingBalance = ?";

    // Bind the parameter for the new starting balance
    retCodeSystem = SQLPrepareW(hStmtSystem, updateQuery, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to prepare update statement", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Set the parameter value
    SQLDOUBLE paramStartingBalance = newStartingBalance;
    retCodeSystem = SQLBindParameter(hStmtSystem, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, &paramStartingBalance, 0, NULL);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Execute the update query
    retCodeSystem = SQLExecute(hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to execute update statement", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return true;
}

bool UpdateBalanceInDatabase(double newBalance) {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Prepare the update query
    WCHAR updateQuery[] = L"UPDATE System SET Balance = ?";

    // Bind the parameter for the new balance
    retCodeSystem = SQLPrepareW(hStmtSystem, updateQuery, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to prepare update statement", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Set the parameter value
    SQLDOUBLE paramBalance = newBalance;
    retCodeSystem = SQLBindParameter(hStmtSystem, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, &paramBalance, 0, NULL);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Execute the update query
    retCodeSystem = SQLExecute(hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to execute update statement", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return true;
}

bool UpdateCardChargesInDatabase(double newCardCharges) {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Prepare the update query
    WCHAR updateQuery[] = L"UPDATE System SET CardCharges = ?";

    // Bind the parameter for the new card charges
    retCodeSystem = SQLPrepareW(hStmtSystem, updateQuery, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to prepare update statement", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Set the parameter value
    SQLDOUBLE paramCardCharges = newCardCharges;
    retCodeSystem = SQLBindParameter(hStmtSystem, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, &paramCardCharges, 0, NULL);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Execute the update query
    retCodeSystem = SQLExecute(hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to execute update statement", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return true;
}

bool UpdateTaxRateInDatabase(double newTaxRate) {
    SQLHENV hEnvSystem = NULL;
    SQLHDBC hDbcSystem = NULL;
    SQLHSTMT hStmtSystem = NULL;
    SQLRETURN retCodeSystem;

    // Initialize the SQL environment handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    SQLSetEnvAttr(hEnvSystem, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

    // Allocate the SQL connection handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_DBC, hEnvSystem, &hDbcSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Connect to the database
    retCodeSystem = SQLDriverConnectW(hDbcSystem, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hDbcSystem, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Allocate the statement handle
    retCodeSystem = SQLAllocHandle(SQL_HANDLE_STMT, hDbcSystem, &hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Prepare the update query
    WCHAR updateQuery[] = L"UPDATE System SET TaxRate = ?";

    // Bind the parameter for the new tax rate
    retCodeSystem = SQLPrepareW(hStmtSystem, updateQuery, SQL_NTS);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to prepare update statement", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Set the parameter value
    SQLDOUBLE paramTaxRate = newTaxRate;
    retCodeSystem = SQLBindParameter(hStmtSystem, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, &paramTaxRate, 0, NULL);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Execute the update query
    retCodeSystem = SQLExecute(hStmtSystem);
    if (!SQL_SUCCEEDED(retCodeSystem)) {
        MessageBox(NULL, L"Failed to execute update statement", L"Error", MB_OK | MB_ICONERROR);
        PrintDiagRec(hStmtSystem, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
        SQLDisconnect(hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);
        return false;
    }

    // Clean up
    SQLFreeHandle(SQL_HANDLE_STMT, hStmtSystem);
    SQLDisconnect(hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbcSystem);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnvSystem);

    return true;
}

INT_PTR CALLBACK EnterStartingBalanceDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 13 for the starting balance edit control
        SendDlgItemMessage(hDlg, IDC_EDIT_STARTING_BALANCE, EM_SETLIMITTEXT, 13, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            WCHAR startingBalanceStr[14]; // 13 characters + 1 for null terminator
            GetDlgItemTextW(hDlg, IDC_EDIT_STARTING_BALANCE, startingBalanceStr, sizeof(startingBalanceStr) / sizeof(WCHAR));

            try {
                // Convert to double
                double newStartingBalance = _wtof(startingBalanceStr);

                // Validate the newStartingBalance
                if (newStartingBalance < 0.0) {
                    throw std::out_of_range("Starting Balance cannot be negative.");
                }
                else if (newStartingBalance > 9999999999.99) {
                    throw std::out_of_range("Starting Balance cannot be greater than 9999999999.99.");
                }

                // Update the starting balance in the database
                if (UpdateStartingBalanceInDatabase(newStartingBalance)) {
                    EndDialog(hDlg, IDOK); // Successfully updated
                }
                else {
                    // Display an error message if the update fails
                    MessageBox(hDlg, L"Failed to update starting balance", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            catch (const std::invalid_argument&) {
                // Handle invalid input
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                // Handle out-of-range errors
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL); // Close dialog without saving
            return TRUE;
        }
        break;
    }

    return FALSE;
}

INT_PTR CALLBACK EnterBalanceDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 15 for the balance edit control
        SendDlgItemMessage(hDlg, IDC_EDIT_BALANCE, EM_SETLIMITTEXT, 15, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            WCHAR balanceStr[16]; // 15 characters + 1 for null terminator
            GetDlgItemTextW(hDlg, IDC_EDIT_BALANCE, balanceStr, sizeof(balanceStr) / sizeof(WCHAR));

            try {
                // Convert to double
                double newBalance = _wtof(balanceStr);

                // Validate the newBalance
                if (newBalance < 0.0) {
                    throw std::out_of_range("Balance cannot be negative.");
                }
                else if (newBalance > 999999999999.99) {
                    throw std::out_of_range("Balance cannot be greater than 999999999999.99.");
                }

                // Update the balance in the database
                if (UpdateBalanceInDatabase(newBalance)) {
                    EndDialog(hDlg, IDOK); // Successfully updated
                }
                else {
                    // Display an error message if the update fails
                    MessageBox(hDlg, L"Failed to update balance", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            catch (const std::invalid_argument&) {
                // Handle invalid input
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                // Handle out-of-range errors
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL); // Close dialog without saving
            return TRUE;
        }
        break;
    }

    return FALSE;
}

INT_PTR CALLBACK EnterTaxRateDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters to 6 for the tax rate edit control
        SendDlgItemMessage(hDlg, IDC_EDIT_TAX_RATE, EM_SETLIMITTEXT, 6, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            WCHAR taxRateStr[7]; // 6 characters + 1 for null terminator
            GetDlgItemTextW(hDlg, IDC_EDIT_TAX_RATE, taxRateStr, sizeof(taxRateStr) / sizeof(WCHAR));

            try {
                // Convert to double
                double newTaxRate = _wtof(taxRateStr);

                // Validate the newTaxRate
                if (newTaxRate < 0.0) {
                    throw std::out_of_range("Tax Rate cannot be negative.");
                }
                else if (newTaxRate > 999.99) {
                    throw std::out_of_range("Tax Rate cannot be greater than 999.99.");
                }

                // Update the tax rate in the database
                if (UpdateTaxRateInDatabase(newTaxRate)) {
                    EndDialog(hDlg, IDOK); // Successfully updated
                }
                else {
                    // Display an error message if the update fails
                    MessageBox(hDlg, L"Failed to update tax rate", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            catch (const std::invalid_argument&) {
                // Handle invalid input
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_NUMBER), hDlg, PopupDlgProc);
            }
            catch (const std::out_of_range&) {
                // Handle out-of-range errors
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INVALID_INPUT_RANGE), hDlg, PopupDlgProc);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL); // Close dialog without saving
            return TRUE;
        }
        break;
    }

    return FALSE;
}

INT_PTR CALLBACK ManagerOptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static const double ERROR_BALANCE = -1.0;
    static double tempStartingBalance = 0;
    static double tempBalance = 0;
    static double tempCardCharges = 0;
    static double tempTaxRate = 0;

    switch (message) {
    case WM_INITDIALOG:
    {
        // Fetch the starting balance, balance, card charges, and tax rate from the database
        double startingBalance = FetchStartingBalanceFromDatabase();
        double balance = FetchBalanceFromDatabase();
        double cardCharges = FetchCardChargesFromDatabase();
        double taxRate = FetchTaxRateFromDatabase();
        tempStartingBalance = startingBalance;
        tempBalance = balance;
        tempCardCharges = cardCharges;
        tempTaxRate = taxRate;

        // Check for error
        if (startingBalance != ERROR_BALANCE) {
            // Convert balance to a string and set it to the static control
            WCHAR startingBalanceStr[50];
            swprintf(startingBalanceStr, sizeof(startingBalanceStr) / sizeof(WCHAR), L"$%.2f", startingBalance);
            SetDlgItemTextW(hDlg, IDC_DISPLAY_START_BALANCE, startingBalanceStr);
        }
        else {
            // Handle the error case
            SetDlgItemTextW(hDlg, IDC_DISPLAY_START_BALANCE, L"Error retrieving");
        }

        // Check for error
        if (balance != ERROR_BALANCE) {
            // Convert balance to a string and set it to the static control
            WCHAR balanceStr[50];
            swprintf(balanceStr, sizeof(balanceStr) / sizeof(WCHAR), L"$%.2f", balance);
            SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, balanceStr);
        }
        else {
            // Handle the error case
            SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, L"Error retrieving");
        }

        // Check for error
        if (cardCharges != ERROR_BALANCE) {
            // Convert balance to a string and set it to the static control
            WCHAR cardChargesStr[50];
            swprintf(cardChargesStr, sizeof(cardChargesStr) / sizeof(WCHAR), L"$%.2f", cardCharges);
            SetDlgItemTextW(hDlg, IDC_DISPLAY_CARD_CHARGES, cardChargesStr);
        }
        else {
            // Handle the error case
            SetDlgItemTextW(hDlg, IDC_DISPLAY_CARD_CHARGES, L"Error retrieving");
        }

        // Check for error
        if (taxRate != ERROR_BALANCE) {
            // Convert balance to a string and set it to the static control
            WCHAR taxRateStr[50];
            swprintf(taxRateStr, sizeof(taxRateStr) / sizeof(WCHAR), L"%.2f%%", taxRate);
            SetDlgItemTextW(hDlg, IDC_DISPLAY_TAX_RATE, taxRateStr);
        }
        else {
            // Handle the error case
            SetDlgItemTextW(hDlg, IDC_DISPLAY_TAX_RATE, L"Error retrieving");
        }

        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_VOID_TRANSACTION:
            g_ItemPrice = -1234; // Triggers emptying of itemList and IDC_ITEM_LIST when returning the Main dialog
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDC_EMPTY_BALANCE:
            // Set the balance to 0.0 in the database
            if (UpdateBalanceInDatabase(0.0)) {
                // Refresh the current balance
                double balance = FetchBalanceFromDatabase();
                if (balance != ERROR_BALANCE) {
                    WCHAR balanceStr[50];
                    swprintf(balanceStr, sizeof(balanceStr) / sizeof(WCHAR), L"$%.2f", balance);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, balanceStr);
                }
            }
            else {
                SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, L"Error updating balance");
            }
            return TRUE;

        case IDC_EMPTY_CARD_CHARGES:
            // Set the card charges to 0.0 in the database
            if (UpdateCardChargesInDatabase(0.0)) {
                // Refresh the current card charges
                double cardCharges = FetchCardChargesFromDatabase();
                if (cardCharges != ERROR_BALANCE) {
                    WCHAR cardChargesStr[50];
                    swprintf(cardChargesStr, sizeof(cardChargesStr) / sizeof(WCHAR), L"$%.2f", cardCharges);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_CARD_CHARGES, cardChargesStr);
                }
            }
            else {
                SetDlgItemTextW(hDlg, IDC_DISPLAY_CARD_CHARGES, L"Error updating card charges");
            }
            return TRUE;

        case IDC_CARD:
            // Set the balance to 0.0 in the database
            if (UpdateBalanceInDatabase(0.0)) {
                // Refresh the current balance
                double balance = FetchBalanceFromDatabase();
                if (balance != ERROR_BALANCE) {
                    WCHAR balanceStr[50];
                    swprintf(balanceStr, sizeof(balanceStr) / sizeof(WCHAR), L"$%.2f", balance);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, balanceStr);
                }
            }
            else {
                SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, L"Error updating balance");
            }
            return TRUE;

        case IDC_STARTING_BALANCE:
            // Open the dialog to enter the new starting balance
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_START_BALANCE), hDlg, EnterStartingBalanceDlgProc) == IDOK) {
                // Refresh the current starting balance
                double startingBalance = FetchStartingBalanceFromDatabase();
                if (startingBalance != ERROR_BALANCE) {
                    WCHAR startingBalanceStr[50];
                    swprintf(startingBalanceStr, sizeof(startingBalanceStr) / sizeof(WCHAR), L"$%.2f", startingBalance);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_START_BALANCE, startingBalanceStr);
                }
            }
            return TRUE;


        case IDC_BALANCE:
            // Open the dialog to enter the new balance
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_BALANCE), hDlg, EnterBalanceDlgProc) == IDOK) {
                // Refresh the current balance
                double balance = FetchBalanceFromDatabase();
                if (balance != ERROR_BALANCE) {
                    WCHAR balanceStr[50];
                    swprintf(balanceStr, sizeof(balanceStr) / sizeof(WCHAR), L"$%.2f", balance);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_BALANCE, balanceStr);
                }
            }
            return TRUE;
        case IDC_TAX_RATE:
            // Open the dialog to enter the new tax rate
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ENTER_TAX_RATE), hDlg, EnterTaxRateDlgProc) == IDOK) {
                // Refresh the current tax rate
                double taxRate = FetchTaxRateFromDatabase();
                if (taxRate != ERROR_BALANCE) {
                    WCHAR taxRateStr[50];
                    swprintf(taxRateStr, sizeof(taxRateStr) / sizeof(WCHAR), L"%.2f%%", taxRate);
                    SetDlgItemTextW(hDlg, IDC_DISPLAY_TAX_RATE, taxRateStr);
                }
            }
            return TRUE;

        case IDOK:
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            // Set starting balance, balance, card charges, and tax rate back to their original values if dialog is cancelled
            UpdateBalanceInDatabase(tempBalance);
            UpdateStartingBalanceInDatabase(tempStartingBalance);
            UpdateCardChargesInDatabase(tempCardCharges);
            UpdateTaxRateInDatabase(tempTaxRate);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowManagerOptionsDialog(HWND hParent) {
    // Show the dialog to edit manager options
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_MANAGER_OPTIONS), hParent, ManagerOptionsDlgProc);
}

INT_PTR CALLBACK ManagerLoginDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    SQLHENV hEnvUsers = NULL;
    SQLHDBC hDbcUsers = NULL;
    SQLHSTMT hStmtUsers = NULL;
    SQLRETURN retCodeUsers;

    switch (message) {
    case WM_INITDIALOG:
        // Set the maximum number of characters for user ID and password inputs
        SendDlgItemMessage(hDlg, IDC_EDIT_USERID, EM_SETLIMITTEXT, 4, 0);
        SendDlgItemMessage(hDlg, IDC_EDIT_PASSWORD, EM_SETLIMITTEXT, 4, 0);

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        {

            // Allocate environment handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnvUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate environment handle", L"Error", MB_OK | MB_ICONERROR);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            SQLSetEnvAttr(hEnvUsers, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_ENSURE);

            // Allocate connection handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_DBC, hEnvUsers, &hDbcUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate connection handle", L"Error", MB_OK | MB_ICONERROR);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Connect to the database
            retCodeUsers = SQLDriverConnectW(hDbcUsers, NULL, (SQLWCHAR*)CONNECTION_STRING, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Connection failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hDbcUsers, SQL_HANDLE_DBC);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Get entered text from edit controls
            WCHAR userIDStr[5]; // 4 characters + null terminator
            WCHAR passwordStr[5]; // 4 characters + null terminator
            GetDlgItemTextW(hDlg, IDC_EDIT_USERID, userIDStr, sizeof(userIDStr) / sizeof(WCHAR));
            GetDlgItemTextW(hDlg, IDC_EDIT_PASSWORD, passwordStr, sizeof(passwordStr) / sizeof(WCHAR));

            // Convert to integers
            int userID = _wtoi(userIDStr);
            int password = _wtoi(passwordStr);

            // Allocate statement handle
            retCodeUsers = SQLAllocHandle(SQL_HANDLE_STMT, hDbcUsers, &hStmtUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to allocate statement handle", L"Error", MB_OK | MB_ICONERROR);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Prepare the query
            retCodeUsers = SQLPrepareW(hStmtUsers, (SQLWCHAR*)QUERY_USERS, SQL_NTS);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Query preparation failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Bind the parameter for the query
            retCodeUsers = SQLBindParameter(hStmtUsers, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &userID, 0, NULL);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind parameter", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Execute the query
            retCodeUsers = SQLExecute(hStmtUsers);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Query execution failed", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            SQLSMALLINT fetchedPassword = 0;
            WCHAR name[21] = { 0 }; // nchar(20) + null terminator
            SQLCHAR managerBit = 0;
            SQLLEN passwordLen = 0, nameLen = 0, managerLen = 0;

            // Bind columns
            retCodeUsers = SQLBindCol(hStmtUsers, 1, SQL_C_SSHORT, &fetchedPassword, 0, &passwordLen);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind column for Password", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            retCodeUsers = SQLBindCol(hStmtUsers, 2, SQL_C_WCHAR, name, sizeof(name), &nameLen);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind column for Name", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            retCodeUsers = SQLBindCol(hStmtUsers, 3, SQL_C_BIT, &managerBit, 0, &managerLen);
            if (!SQL_SUCCEEDED(retCodeUsers)) {
                MessageBox(NULL, L"Failed to bind column for Manager", L"Error", MB_OK | MB_ICONERROR);
                PrintDiagRec(hStmtUsers, SQL_HANDLE_STMT);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
                SQLDisconnect(hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
                SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }

            // Fetch data
            int isValid = 0;
            while (SQLFetch(hStmtUsers) == SQL_SUCCESS) {
                if (passwordLen > 0 && fetchedPassword == password) {
                    // Check if the user is a manager
                    if (managerBit == 1)
                    {
                        isValid = 2;
                    }
                    else
                    {
                        isValid = 1;
                    }
                    break;
                }
            }

            // Clean up
            SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
            SQLDisconnect(hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);

            // Determine dialog result
            if (isValid == 2) {
                EndDialog(hDlg, IDOK);
            }
            else if (isValid == 1) {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_NOT_MANAGER), hDlg, PopupDlgProc);
            }
            else {
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INCORRECT), hDlg, PopupDlgProc);
            }
        }
        return TRUE;

        case IDCANCEL:
            // Cleanup and close the dialog
            SQLFreeHandle(SQL_HANDLE_STMT, hStmtUsers);
            SQLDisconnect(hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbcUsers);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnvUsers);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowManagerLoginDialog(HWND hParent) {
    // Show the dialog to get user ID and password
    int result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_MANAGER_LOGIN), hParent, ManagerLoginDlgProc);

    if (result == IDOK) {
        ShowManagerOptionsDialog(hParent);
    }
    else {
        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_INCORRECT), hParent, PopupDlgProc);
    }
}

INT_PTR CALLBACK PayDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        HWND hTotalList = GetDlgItem(hDlg, IDC_TOTAL_LIST);
        SetListBoxFont(hTotalList); // Set the font for the total list box
        UpdateTotalDisplay(hTotalList); // Update total display
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CASH:
            UpdateBalanceInDatabase((FetchBalanceFromDatabase() + g_Total)); // Add total to balance
            g_ItemPrice = -1234; // Triggers emptying of itemList and IDC_ITEM_LIST when returning the Main dialog
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_PAYMENT_SUCCESS), hDlg, PopupDlgProc);
            EndDialog(hDlg, IDC_CASH);
            return TRUE;

        case IDC_CARD:
            UpdateCardChargesInDatabase((FetchCardChargesFromDatabase() + g_Total)); // Add total to card charges
            g_ItemPrice = -1234; // Triggers emptying of itemList and IDC_ITEM_LIST when returning the Main dialog
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_PAYMENT_SUCCESS), hDlg, PopupDlgProc);
            EndDialog(hDlg, IDC_CARD);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

void ShowPayDialog(HWND hParent) {
    // Show the dialog to select Cash or Card
    int result = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_PAY), hParent, PayDlgProc);
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        HWND hListBox = GetDlgItem(hDlg, IDC_ITEM_LIST);
        SetListBoxFont(hListBox); // Set the font for the item list box

        HWND hHeader = GetDlgItem(hDlg, IDC_HEADER);
        SetListBoxFont(hHeader); // Set the font for the header
        SendMessage(hHeader, LB_ADDSTRING, 0, (LPARAM)L"Name                   Quantity         Price            Tax");

        HWND hTotalList = GetDlgItem(hDlg, IDC_TOTAL_LIST);
        SetListBoxFont(hTotalList); // Set the font for the total list box
        UpdateTotalDisplay(hTotalList);

        UpdateBalanceInDatabase(FetchStartingBalanceFromDatabase()); // Set balance to starting balance
        UpdateCardChargesInDatabase(0.0); // Set card charges to 0.0
        return TRUE;
    }
    return TRUE;

    case WM_COMMAND:
    {
        HWND hListBox = GetDlgItem(hDlg, IDC_ITEM_LIST);
        HWND hTotalList = GetDlgItem(hDlg, IDC_TOTAL_LIST);
        if (hListBox) {
            switch (LOWORD(wParam)) {
            case IDC_LOOKUP:
                ShowLookupDialog(hDlg);
                break;

            case IDC_ENTER_ITEM:
                ShowEnterItemDialog(hDlg);
                break;

            case IDC_REMOVE_ITEM:
            {
                if (ShowEmployeeLoginDialog(hDlg)) {
                    // Retrieve the index of the currently selected item
                    int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);

                    // Check if a valid item is selected
                    if (selectedIndex != LB_ERR) {
                        // Ensure that the index is within bounds of the itemList
                        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(itemList.size())) {
                            // Remove the item from the itemList vector
                            itemList.erase(itemList.begin() + selectedIndex);

                            // Remove the item from the list box
                            SendMessage(hListBox, LB_DELETESTRING, selectedIndex, 0);

                            // Update total display
                            UpdateTotalDisplay(hTotalList);
                        }
                        else {
                            MessageBox(hDlg, L"Selected index is out of range", L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                    else {
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_NO_SELECTION), hDlg, PopupDlgProc);
                    }
                }
                break;
            }

            case IDC_PAY:
                if (!itemList.empty()) {
                    ShowPayDialog(hDlg);
                    if (g_ItemPrice == -1234) {
                        SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
                        itemList.clear();
                    }
                    UpdateTotalDisplay(hTotalList);
                }
                else
                {
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_NO_ITEMS), hDlg, PopupDlgProc);
                }
                break;

            case IDC_MANAGER_OPTIONS:
                ShowManagerLoginDialog(hDlg);
                if (g_ItemPrice == -1234) {
                    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
                    itemList.clear();
                }
                UpdateTotalDisplay(hTotalList);
                break;

            case IDCANCEL: // Exit program with ESC
                EndDialog(hDlg, 0);
                return TRUE;
            }
        }
    }
    break;

    case WM_UPDATE_LIST:
    {
        HWND hListBox = GetDlgItem(hDlg, IDC_ITEM_LIST);
        HWND hTotalList = GetDlgItem(hDlg, IDC_TOTAL_LIST);
        if (hListBox) {
            std::ostringstream oss;
            std::ostringstream quantityStream;
            if (g_ItemWeight) {
                quantityStream << std::fixed << std::setprecision(2) << g_ItemQuantity << " lbs";
            }
            else
            {
                quantityStream << std::fixed << std::setprecision(0) << g_ItemQuantity;
            }
            oss << std::left << std::setw(23) << g_ItemName
                << std::setw(17) << std::fixed << std::setprecision(2) << quantityStream.str()
                << "$" << std::setw(16) << std::fixed << std::setprecision(2) << g_ItemPrice;
            if (g_ItemTax) {
                oss << " T";
            }

            // Add new entry to the bottom of the list
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)oss.str().c_str());

            // Add item to the item list
            itemList.push_back({ g_ItemName, g_ItemPrice, g_ItemQuantity, g_ItemTax });

            // Update total display
            UpdateTotalDisplay(hTotalList);
        }
    }
    return TRUE;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return TRUE;
    }

    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, DlgProc);
}
