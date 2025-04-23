# Self Service Point of Sale
Created in 2024 as my senior capstone project to complete my bachelor's degree, this application is a point-of-sale program for a self service check out machine operated by a customer to ring up and pay for their items in a grocery store. The interface was designed with big buttons to be touch screen friendly, and has very few commands. Every item in the database can be looked up using the “Lookup” button or entered using the “Enter Item #” button. Clicking “Remove Item” will prompt for an employee login to authorize the void. There is also a “Manager Options” menu to track the cash balance, tax rate, and card charges that have occurred since the program initialized. When the user selects “Pay” they can choose either a “Cash” or “Card” transaction, and this is added to the System database to keep a total of all transactions of each type since the program was last started.  

## Main Screen Example
<img src="https://github.com/user-attachments/assets/dee261c2-e2ad-4b28-8820-15b422047a69" width="800">

## Example Video
[![Link to YouTube](https://github.com/user-attachments/assets/ce9971c3-d0e3-4022-8910-f36999c5dbbd)](https://www.youtube.com/watch?v=gT5bKg5IO3s)

## Behind the Scenes
The program was created in C++ utilizing Visual Studio as a Win32 application, also known as the Windows API. As such, the program runs on a Microsoft Windows computer. The program interfaces with an SQL database by the name of PointOfSale containing three tables: Products, System, and Users.

### System table
![image](https://github.com/user-attachments/assets/4332aadb-06e1-4e61-826f-d7daee5b6952)  
The program only uses the first row in each column to store information used by the system.

### Users table
![image](https://github.com/user-attachments/assets/ee6f1895-4fd0-47b5-a940-3cf4b100d5be)  
ID acts a primary key for each user.

### Products table
![image](https://github.com/user-attachments/assets/23fa097d-2bea-4fb1-8f8c-7db3ecfd7ad7)  
Code acts as a primary key for each product, representing a barcode or PLU.

## Functions
The program has many functions, mostly corresponding with each of these dialog boxes. In order, the functions are as follows:
+ SetListBoxFont – Sets the font for all item and price displays to a monospaced large font
+ PrintDiagRec – Prints SQL errors if any occur
+ PopupDlgProc – Displays error dialogs such as “ITEM NOT FOUND” and many more
+ FetchStartingBalanceFromDatabase – Retrieves the starting cash balance from the System database
+ FetchBalanceFromDatabase – Retrieves the current cash balance from the System database
+ FetchCardChargesFromDatabase – Retrieves the current total card charges from the System database
+ FetchTaxRateFromDatabase – Retrieves the tax rate from the System database
+ UpdateTotalDisplay – Calculates subtotal, tax, and total based on the items that have been entered by the user and displays them
+ EnterWeightDlgProc – Handles user input of an item weight
+ EnterQuantityDlgProc – Handles user input of an item quantity
+ RetrieveItemData – Retrieves an item’s name, price, if it is weighable, and if it is taxable from the Products database from a given item code
+ RetrieveItemsByFirstLetter – Retrieves items that start with a given letter of the alphabet, used to create an alphabetized lookup list of products for use in the Item Lookup dialog
+ ItemLookupDlgProc – Handles creating the list of items that start with a given letter
+ ShowItemLookupDialog – Prints the list created by the last two functions to the Item Lookup dialog
+ ShowLookupDialog – Shows the Lookup dialog, which contains buttons A through Z which then proceeds to the Item Lookup dialog, passing the letter selected to the previous three functions
+ ItemEntryDlgProc – Handles logic from the Enter Item dialog, passing the code the user enters to RetrieveItemData and displaying errors if necessary
+ ShowEnterItemDialog – Display Enter Item dialog
+ EmployeeLoginDlgProc – Handles input of Employee Login information and compares to ID and Password in Users database, used when Remove Item is selected by user
+ ShowEmployeeLoginDialog – Displays Employee Logic dialog
+ UpdateStartingBalanceInDatabase – Writes starting balance to System database
+ UpdateBalanceInDatabase– Writes cash balance to System database
+ UpdateCardChargesInDatabase– Writes card charges to System database
+ UpdateTaxRateInDatabase– Writes tax rate to System database
+ EnterStartingBalanceDlgProc – Handles logic of inputting starting balance, sets limits, and displays errors if necessary
+ EnterBalanceDlgProc – Handles logic of inputting balance, sets limits, and displays errors if necessary
+ EnterTaxRateDlgProc – Handles logic of inputting tax rates, sets limits, and displays errors if necessary
+ ShowManagerOptionsDialog – Displays Manager Options dialog
+ ManagerOptionsDlgProc – Handles logic of Manager Options including displaying and setting values in System database, voiding transactions, and emptying cash balance and card charges total
+ ShowManagerLoginDialog – Displays Manager Logic dialog
+ ManagerLoginDlgProc – Handles input of Manager Login information and compares to ID and Password in Users database, used when Manager Options is selected. Displays error if user is found but not a manager
+ PayDlgProc – Handles updating cash balance or card charges when user selected Cash or Card and clears transaction afterwards
+ ShowPayDialog – Shows Pay dialog
+ DlgProc – Main procedure, handles the default screen “Main” which displays all items entered and has the buttons Lookup, Enter Item #, Remove, Manager Options, and Pay. Handles logic of clicking each of these. Displays a list of items entered in the middle with a monoscape large font. Item list is selectable with the mouse cursor. When Remove Item is pressed it removes the selected item from the list. Uses UpdateTotalDisplay every time an item is input or removed to display subtotal, tax, and total at the bottom of the Main dialog
+ WinMain – Calls the Main dialog
