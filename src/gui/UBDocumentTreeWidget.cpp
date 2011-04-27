/*
 * UBDocumentTreeWidget.cpp
 *
 *  Created on: Dec 9, 2008
 *      Author: Luc
 */

#include "UBDocumentTreeWidget.h"

#include "document/UBDocumentProxy.h"

#include "core/UBSettings.h"
#include "core/UBApplication.h"
#include "core/UBPersistenceManager.h"
#include "core/UBMimeData.h"
#include "core/UBApplicationController.h"
#include "core/UBDocumentManager.h"
#include "document/UBDocumentController.h"


UBDocumentTreeWidget::UBDocumentTreeWidget(QWidget * parent)
    : QTreeWidget(parent)
    , mSelectedProxyTi(0)
    , mDropTargetProxyTi(0)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setAutoScroll(true);

    connect(UBDocumentManager::documentManager(), SIGNAL(documentUpdated(UBDocumentProxy*))
            , this, SLOT(documentUpdated(UBDocumentProxy*)));

    connect(this, SIGNAL(itemChanged(QTreeWidgetItem *, int))
            , this,  SLOT(itemChangedValidation(QTreeWidgetItem *, int)));
}


UBDocumentTreeWidget::~UBDocumentTreeWidget()
{
    // NOOP
}


void UBDocumentTreeWidget::itemChangedValidation(QTreeWidgetItem * item, int column)
{
    if (column == 0)
    {
        UBDocumentGroupTreeItem *group = dynamic_cast< UBDocumentGroupTreeItem *>(item);

        if (group)
        {
            QString name = group->text(0);

            for(int i = 0; i < topLevelItemCount (); i++)
            {
                QTreeWidgetItem *someTopLevelItem = topLevelItem(i);

                if (someTopLevelItem != group &&
                        someTopLevelItem->text(0) == name)
                {
                    group->setText(0, tr("%1 (copy)").arg(name));
                }
            }
        }
    }
}


Qt::DropActions UBDocumentTreeWidget::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}


void UBDocumentTreeWidget::mousePressEvent(QMouseEvent *event)
{
    QTreeWidgetItem* twItem = this->itemAt(event->pos());

    mSelectedProxyTi = dynamic_cast<UBDocumentProxyTreeItem*>(twItem);

    QTreeWidget::mousePressEvent(event);
}


void UBDocumentTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}


void UBDocumentTreeWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);

    if (mDropTargetProxyTi)
    {
        mDropTargetProxyTi->setBackground(0, mBackground);
        mDropTargetProxyTi = 0;
    }
}


void UBDocumentTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeWidgetItem* underlyingItem = this->itemAt(event->pos());

    if (event->mimeData()->hasFormat(UBApplication::mimeTypeUniboardPage))
    {
        UBDocumentProxyTreeItem *targetProxyTreeItem = dynamic_cast<UBDocumentProxyTreeItem*>(underlyingItem);
        if (targetProxyTreeItem && targetProxyTreeItem != mSelectedProxyTi)
        {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        }
        else
        {
            event->ignore();
        }
    }
    else
    {
        UBDocumentGroupTreeItem *groupItem = dynamic_cast<UBDocumentGroupTreeItem*>(underlyingItem);

        if (groupItem && mSelectedProxyTi && groupItem != mSelectedProxyTi->parent())
            event->acceptProposedAction();
        else
            event->ignore();
    }

    if (event->isAccepted())
    {
        if (mDropTargetProxyTi)
        {
            if (underlyingItem != mDropTargetProxyTi)
            {
                mBackground = underlyingItem->background(0);
                mDropTargetProxyTi->setBackground(0, mBackground);
                mDropTargetProxyTi = underlyingItem;
                mDropTargetProxyTi->setBackground(0, QBrush(QColor("#6682b5")));
            }
        }
        else
        {
            mBackground = underlyingItem->background(0);
            mDropTargetProxyTi = underlyingItem;
            mDropTargetProxyTi->setBackground(0, QBrush(QColor("#6682b5")));
        }
    }
    else if (mDropTargetProxyTi)
    {
        mDropTargetProxyTi->setBackground(0, mBackground);
        mDropTargetProxyTi = 0;
    }
}


void UBDocumentTreeWidget::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);

    itemSelectionChanged();
    QTreeWidget::focusInEvent(event);
}


void UBDocumentTreeWidget::dropEvent(QDropEvent *event)
{
    if (mDropTargetProxyTi)
    {
        mDropTargetProxyTi->setBackground(0, mBackground);
        mDropTargetProxyTi = 0;
    }

    QTreeWidgetItem* underlyingItem = this->itemAt(event->pos());

    UBDocumentGroupTreeItem *groupItem = dynamic_cast<UBDocumentGroupTreeItem*>(underlyingItem);

    if (groupItem && mSelectedProxyTi && mSelectedProxyTi->proxy())
    {
        UBDocumentGroupTreeItem *sourceGroupItem = dynamic_cast<UBDocumentGroupTreeItem*>(mSelectedProxyTi->parent());
        bool isTrashItem = sourceGroupItem && sourceGroupItem->isTrashFolder();
        if ((isTrashItem && !groupItem->isTrashFolder()) ||
            (!isTrashItem && mSelectedProxyTi->proxy()->groupName() != groupItem->groupName()))
        {
            QString groupName;
            if (groupItem->isTrashFolder())
            {
                QString oldGroupName = mSelectedProxyTi->proxy()->metaData(UBSettings::documentGroupName).toString();
                groupName = UBSettings::trashedDocumentGroupNamePrefix + oldGroupName;
            }
            else
            {
                if (groupItem->groupName() == UBSettings::defaultDocumentGroupName)
                    groupName = "";
                else
                    groupName = groupItem->groupName();
            }
            mSelectedProxyTi->proxy()->setMetaData(UBSettings::documentGroupName, groupName);
            UBPersistenceManager::persistenceManager()->persistDocumentMetadata(mSelectedProxyTi->proxy());

            mSelectedProxyTi->parent()->removeChild(mSelectedProxyTi);

            int i = 0;
            for (i = 0; i < groupItem->childCount(); i++)
            {
                QTreeWidgetItem *ti = groupItem->child(i);
                UBDocumentProxyTreeItem* pi = dynamic_cast<UBDocumentProxyTreeItem*>(ti);
                if (pi)
                {
                    if (mSelectedProxyTi->proxy()->metaData(UBSettings::documentUpdatedAt).toString() >= pi->proxy()->metaData(UBSettings::documentUpdatedAt).toString())
                    {
                        break;
                    }
                }
            }
            groupItem->insertChild(i, mSelectedProxyTi);

            if (isTrashItem)
                mSelectedProxyTi->setFlags(mSelectedProxyTi->flags() | Qt::ItemIsEditable);

            if (groupItem->isTrashFolder())
                mSelectedProxyTi->setFlags(mSelectedProxyTi->flags() ^ Qt::ItemIsEditable);

            //clearSelection();
            expandItem(groupItem);
            scrollToItem(mSelectedProxyTi);

            // disabled, as those 2 calls are buggy on windows, the item disappears if we selected them
            //
            setCurrentItem(mSelectedProxyTi);
            mSelectedProxyTi->setSelected(true);

            event->setDropAction(Qt::IgnoreAction);
            event->accept();
        }
    }
    else
    {
        QTreeWidgetItem* underlyingTreeItem = this->itemAt(event->pos());

        UBDocumentProxyTreeItem *targetProxyTreeItem = dynamic_cast<UBDocumentProxyTreeItem*>(underlyingTreeItem);
        if (targetProxyTreeItem && targetProxyTreeItem != mSelectedProxyTi)
        {
            if (event->mimeData()->hasFormat(UBApplication::mimeTypeUniboardPage))
            {
                event->setDropAction(Qt::CopyAction);
                event->accept();

                const UBMimeData *mimeData = qobject_cast <const UBMimeData*>(event->mimeData());

                if (mimeData && mimeData->items().size() > 0)
                {
                        int count = 0;
                        int total = mimeData->items().size();

                        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

                    foreach (UBMimeDataItem sourceItem, mimeData->items())
                    {
                        count++;

                        UBApplication::applicationController->showMessage(tr("Copying page %1/%2").arg(count).arg(total), true);

                        // TODO UB 4.x Move following code to some controller class
                        UBGraphicsScene *scene = UBPersistenceManager::persistenceManager()->loadDocumentScene(sourceItem.documentProxy(), sourceItem.sceneIndex());
                        if (scene)
                        {
                            UBGraphicsScene* sceneClone = scene->sceneDeepCopy();

                            UBDocumentProxy *targetDocProxy = targetProxyTreeItem->proxy();

                            foreach (QUrl relativeFile, scene->relativeDependencies())
                            {
                                QString source = scene->document()->persistencePath() + "/" + relativeFile.toString();
                                QString target = targetDocProxy->persistencePath() + "/" + relativeFile.toString();

                                QFileInfo fi(target);
                                QDir d = fi.dir();

                                d.mkpath(d.absolutePath());
                                QFile::copy(source, target);
                            }

                            UBPersistenceManager::persistenceManager()->insertDocumentSceneAt(targetDocProxy, sceneClone, targetDocProxy->pageCount());
                        }
                    }

                    QApplication::restoreOverrideCursor();

                    UBApplication::applicationController->showMessage(tr("%1 pages copied", "", total).arg(total), false);
                }
            }
            else
            {
                event->setDropAction(Qt::IgnoreAction);
                event->ignore();
            }
        }
    }
}


void UBDocumentTreeWidget::documentUpdated(UBDocumentProxy *pDocument)
{
    UBDocumentProxyTreeItem *treeItem = UBApplication::documentController->findDocument(pDocument);
    if (treeItem)
    {
        QTreeWidgetItem * parent = treeItem->parent();

        if (parent)
        {
            for (int i = 0; i < parent->indexOfChild(treeItem); i++)
            {
                QTreeWidgetItem *ti = parent->child(i);
                UBDocumentProxyTreeItem* pi = dynamic_cast<UBDocumentProxyTreeItem*>(ti);
                if (pi)
                {
                    if (pDocument->metaData(UBSettings::documentUpdatedAt).toString() >= pi->proxy()->metaData(UBSettings::documentUpdatedAt).toString())
                    {
                        bool selected = treeItem->isSelected();
                        parent->removeChild(treeItem);
                        parent->insertChild(i, treeItem);
                        for (int j = 0; j < selectedItems().count(); j++)
                            selectedItems().at(j)->setSelected(false);
                        if (selected)
                            treeItem->setSelected(true);
                        break;
                    }
                }
            }
        }
    }
}


UBDocumentProxyTreeItem::UBDocumentProxyTreeItem(QTreeWidgetItem * parent, UBDocumentProxy* proxy, bool isEditable)
    : QTreeWidgetItem()
    , mProxy(proxy)
{
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;

    if (isEditable)
        flags |= Qt::ItemIsEditable;

    setFlags(flags);

    int i = 0;
    for (i = 0; i < parent->childCount(); i++)
    {
        QTreeWidgetItem *ti = parent->child(i);
        UBDocumentProxyTreeItem* pi = dynamic_cast<UBDocumentProxyTreeItem*>(ti);
        if (pi)
        {
            if (proxy->metaData(UBSettings::documentUpdatedAt).toString() >= pi->proxy()->metaData(UBSettings::documentUpdatedAt).toString())
            {
                break;
            }
        }
    }
    parent->insertChild(i, this);
}


UBDocumentGroupTreeItem::UBDocumentGroupTreeItem(QTreeWidgetItem *parent, bool isEditable)
    : QTreeWidgetItem(parent)
{
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    if (isEditable)
        flags |= Qt::ItemIsEditable;
    setFlags(flags);
}


UBDocumentGroupTreeItem::~UBDocumentGroupTreeItem()
{
    // NOOP
}


void UBDocumentGroupTreeItem::setGroupName(const QString& groupName)
{
    setText(0, groupName);
}


QString UBDocumentGroupTreeItem::groupName() const
{
    return text(0);
}


bool UBDocumentGroupTreeItem::isTrashFolder() const
{
    return (0 == (flags() & Qt::ItemIsEditable)) && (groupName() == UBSettings::documentTrashGroupName);
}

bool UBDocumentGroupTreeItem::isDefaultFolder() const
{
    return (0 == (flags() & Qt::ItemIsEditable)) && (groupName() == UBSettings::defaultDocumentGroupName);
}