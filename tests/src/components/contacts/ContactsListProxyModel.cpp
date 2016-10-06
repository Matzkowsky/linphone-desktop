#include <QDebug>

#include "ContactsListProxyModel.hpp"

#define USERNAME_WEIGHT 50.0
#define MAIN_SIP_ADDRESS_WEIGHT 30.0
#define OTHER_SIP_ADDRESSES_WEIGHT 20.0

#define FACTOR_POS_1 0.90
#define FACTOR_POS_2 0.80
#define FACTOR_POS_3 0.70
#define FACTOR_POS_OTHER 0.60

// ===================================================================

ContactsListModel *ContactsListProxyModel::m_list = nullptr;

// Notes:
//
// - First `^` is necessary to search two words with one separator
// between them like `Claire Manning`.
//
// - [^_.-;@ ] is used to search patterns which starts with
// a separator like ` word`.
//
// - [_.-;@ ] is the main pattern (a separator).
const QRegExp ContactsListProxyModel::m_search_separators("^[^_.-;@ ][_.-;@ ]");

// -------------------------------------------------------------------

ContactsListProxyModel::ContactsListProxyModel (QObject *parent) : QSortFilterProxyModel(parent) {
  setSourceModel(m_list);
  setFilterCaseSensitivity(Qt::CaseInsensitive);

  foreach (const ContactModel *contact, m_list->m_list)
    m_weights[contact] = 0;

  sort(0);
}

void ContactsListProxyModel::initContactsListModel (ContactsListModel *list) {
  if (!m_list)
    m_list = list;
  else
    qWarning() << "Contacts list model is already defined.";
}

bool ContactsListProxyModel::filterAcceptsRow (int source_row, const QModelIndex &source_parent) const {
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
  const ContactModel *contact = qvariant_cast<ContactModel *>(
    index.data()
  );

  int weight = m_weights[contact] = static_cast<int>(
    computeContactWeight(*contact)
  );

  return weight > 0 && (
    !m_use_connected_filter ||
    contact->getPresenceLevel() != Presence::PresenceLevel::White
  );
}

bool ContactsListProxyModel::lessThan (const QModelIndex &left, const QModelIndex &right) const {
  const ContactModel *contact_a = qvariant_cast<ContactModel *>(
    sourceModel()->data(left)
  );
  const ContactModel *contact_b = qvariant_cast<ContactModel *>(
    sourceModel()->data(right)
  );

  float weight_a = m_weights[contact_a];
  float weight_b = m_weights[contact_b];

  // Sort by weight and name.
  return (
    weight_a > weight_b || (
      weight_a == weight_b &&
      contact_a->m_username <= contact_b->m_username
    )
  );
}

// -------------------------------------------------------------------

float ContactsListProxyModel::computeStringWeight (const QString &string, float percentage) const {
  int index = -1;
  int offset = -1;

  // Search pattern.
  while ((index = filterRegExp().indexIn(string, index + 1)) != -1) {
    // Search n chars between one separator and index.
    int tmp_offset = index - string.lastIndexOf(m_search_separators, index) - 1;

    if ((tmp_offset != -1 && tmp_offset < offset) || offset == -1)
      if ((offset = tmp_offset) == 0) // Little optimization.
        break;
  }

  // No weight.
  if (offset == -1)
    return 0;

  // Weight & offset.
  switch (offset) {
    case 0: return percentage;
    case 1: return percentage * FACTOR_POS_1;
    case 2: return percentage * FACTOR_POS_2;
    case 3: return percentage * FACTOR_POS_3;
    default: break;
  }

  return percentage * FACTOR_POS_OTHER;
}

float ContactsListProxyModel::computeContactWeight (const ContactModel &contact) const {
  float weight = computeStringWeight(contact.m_username, USERNAME_WEIGHT);

  // It exists at least one sip address.
  const QStringList &addresses = contact.m_sip_addresses;
  weight += computeStringWeight(addresses[0], MAIN_SIP_ADDRESS_WEIGHT);

  int size = addresses.size();

  if (size > 1)
    for (auto it = ++addresses.constBegin(); it != addresses.constEnd(); ++it)
      weight += computeStringWeight(*it, OTHER_SIP_ADDRESSES_WEIGHT / size);

  return weight;
}

// -------------------------------------------------------------------

bool ContactsListProxyModel::isConnectedFilterUsed () const {
  return m_use_connected_filter;
}

void ContactsListProxyModel::setConnectedFilter (bool useConnectedFilter) {
  m_use_connected_filter = useConnectedFilter;
  qDebug() << useConnectedFilter;
  invalidate();
}
