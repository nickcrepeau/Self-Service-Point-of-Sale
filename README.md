# Self Service Point of Sale
This application is a point-of-sale program for a self service check out machine operated by a customer to ring up and pay for their items in a grocery store. The interface was designed with big buttons to be touch screen friendly, and has very few commands. Every item in the database can we looked up using the “Lookup” button or entered using the “Enter Item #” button. Clicking “Remove Item” will prompt for an employee login to authorize the void. There is also a “Manager Options” menu to track the cash balance, tax rate, and card charges that have occurred since the program initialized. When the user selects “Pay” they can choose either a Cash or Card transaction, and this is added to the System database to keep a total of all transactions of each type since the program was last started.  

## Main Screen Example
<img src="https://github.com/user-attachments/assets/dee261c2-e2ad-4b28-8820-15b422047a69" width="800">

## Example video
[![Link to YouTube](https://github.com/user-attachments/assets/ce9971c3-d0e3-4022-8910-f36999c5dbbd)](https://www.youtube.com/watch?v=gT5bKg5IO3s)

## Behind the scenes
The program was created in C++ utilizing Visual Studio as a Win32 application, also known as the Windows API. As such, the program runs on a Microsoft Windows computer. The program interfaces with an SQL database by the name of PointOfSale containing three tables: Products, System, and Users.
### System table
![image](https://github.com/user-attachments/assets/4332aadb-06e1-4e61-826f-d7daee5b6952)  
The program only uses the first row in each column to store information used by the system.
### Users table
![image](https://github.com/user-attachments/assets/ee6f1895-4fd0-47b5-a940-3cf4b100d5be)  
ID acts a primary key for each user.
### Products table
![image](https://github.com/user-attachments/assets/23fa097d-2bea-4fb1-8f8c-7db3ecfd7ad7)
