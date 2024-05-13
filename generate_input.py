import random
MAX_DEALS = 4

values = [2, 3, 4, 5, 6, 7, 8, 9, 10, 'J', 'Q', 'K', 'A']
colors = ['H', 'D', 'C', 'S']
cards = []
for value in values:
    for color in colors:
        cards.append(str(value) + color)
seats = ['N', 'E', 'S', 'W']

deals_nr = random.randint(1, MAX_DEALS)

for i in range(deals_nr):
    deal_type = random.randint(1, 7)
    first_seat = seats[random.randint(0, 3)]
    print(str(deal_type) + first_seat)
    random.shuffle(cards)
    cards1 = cards[:13]
    cards2 = cards[13:26]
    cards3 = cards[26:39]
    cards4 = cards[39:52]
    for card in cards1:
        print(card, end='')
    print()
    for card in cards2:
        print(card, end='')
    print()
    for card in cards3:
        print(card, end='')
    print()
    for card in cards4:
        print(card, end='')
    print()