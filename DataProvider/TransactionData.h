#ifndef TRANSACTIONDATA_H
#define TRANSACTIONDATA_H

#include <QDate>
#include <QVariant>

/**
 * @brief Object of those class are used for computation. 3 related values needed.
 */
class TransactionData
{
public:
    TransactionData() = default;

    ~TransactionData() = default;

    TransactionData& operator=(const TransactionData& other) = default;
    TransactionData(const TransactionData& other) = default;

    TransactionData& operator=(TransactionData&& other) = default;
    TransactionData(TransactionData&& other) = default;

    QDate date_;

    QVariant groupedBy_;

    float pricePerMeter_ {0.f};

    void clear();

    void print() const;
};

#endif // TRANSACTIONDATA_H
