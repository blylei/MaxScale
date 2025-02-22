/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-03-24
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

import { expect } from 'chai'
import mount from '@tests/unit/setup'
import DataTable from '@/components/common/DataTable'

const keepPrimitiveValue = true
let rowspanTableHeaders = [
    { text: `Monitor`, value: 'groupId' },
    { text: 'State', value: 'monitorState' },
    { text: 'Servers', value: 'id' },
    { text: 'Address', value: 'serverAddress' },
    { text: 'Port', value: 'serverPort' },
    { text: 'Connections', value: 'serverConnections' },
    { text: 'State', value: 'serverState' },
    { text: 'GTID', value: 'gtid' },
    { text: 'Services', value: 'serviceIds' },
]
let rowspanTableData = [
    {
        id: 'row_server_2',
        serverAddress: '127.0.0.1',
        serverPort: 4002,
        serverConnections: 0,
        serverState: 'Slave, Running',
        serviceIds: ['RCR-Router', 'RCR-Writer', 'RWS-Router'],
        gtid: '0-1000-9',
        groupId: 'Monitor', // will be assigned rowspan 2 via handleDisplayRowspan
        monitorState: 'Running', //rowspan 2
    },
    {
        id: 'row_server_1',
        serverAddress: '127.0.0.1',
        serverPort: 4001,
        serverConnections: 0,
        serverState: 'Master, Running',
        serviceIds: ['RCR-Router', 'RCR-Writer', 'RWS-Router'],
        gtid: '0-1000-9',
        groupId: 'Monitor', // will be assigned rowspan 2 via handleDisplayRowspan
        monitorState: 'Running', //rowspan 2
    },
    {
        id: 'row_server_3',
        serverAddress: '127.0.0.1',
        serverPort: 4003,
        serverConnections: 0,
        serverState: 'Down',
        serviceIds: 'No services',
        gtid: null,
        groupId: 'monitor-test', // will be assigned rowspan 1 via handleDisplayRowspan
        monitorState: 'Running', //rowspan 1
    },
]
/**
 * This function mockup processing tree data
 * @param {Object} wrapper A Wrapper is an object that contains a mounted component and methods to test the component
 * @return {Array} Tree data array that is used to display in data-table
 */
function mockupTreeData(wrapper) {
    /* Tree props data*/
    const objParameter = {
        root_node: {
            node_child: { grand_child: { great_grand_child: 'great_grand_child' } },
            node_child_1: 'node_child_1 value',
        },
    }

    return wrapper.vm.$help.objToTree({ obj: objParameter, keepPrimitiveValue, level: 0 })
}
describe('DataTable.vue', () => {
    let wrapper
    beforeEach(() => {
        localStorage.clear()
        wrapper = mount({
            shallow: false,
            component: DataTable,
            props: {
                /*
                    object in headers may have these properties:
                    sortable(true || false), editableCol (true || false), align ("center || left || right"),
                    cellTruncated (true || false), width (String), padding (String)

                    - editableCol, align is mainly used for styling purpose which is applied in table-cell.
                    - sortable is used for disable sorting on table, by default it's always true.
                    - cellTruncated is a condition to emit 'get-truncated-info' event in table-cell
                */
                headers: [
                    { text: 'Variable', value: 'id' },
                    { text: 'Value', value: 'value' },
                ],
                data: [
                    {
                        id: 'Item 0',
                        value: null,
                    },
                    {
                        id: 'Item 1',
                        value: undefined,
                    },
                    {
                        id: 'Item 2',
                        value: 'value of item 2',
                    },
                ],
            },
        })
    })

    it(`Should process data as expected when keepPrimitiveValue
        props is true or false`, async () => {
        /*
            by default keepPrimitiveValue is set to false which means
            null, undefined become a string i.e. 'null', 'undefined' respectively.
            This allows null or undefined value to be shown on the table and can be searched using global-search
            component
        */
        expect(wrapper.vm.$props.keepPrimitiveValue).to.equal(false)
        let processedData = wrapper.vm.processingData

        expect(processedData[0].value).to.equal('null')
        expect(processedData[1].value).to.equal('undefined')
        expect(processedData[0].value).to.be.a('string')
        expect(processedData[1].value).to.be.a('string')

        // this keep original value
        await wrapper.setProps({
            keepPrimitiveValue: keepPrimitiveValue,
        })
        expect(wrapper.vm.$props.keepPrimitiveValue).to.equal(true)
        let oriData = wrapper.vm.processingData
        expect(oriData[0].value).to.be.a('null')
        expect(oriData[1].value).to.be.an('undefined')
    })

    it(`Tree data table:
      - It processes data as expected when isTree props is true.`, async () => {
        // check default value isTree, should be false
        expect(wrapper.vm.$props.isTree).to.equal(false)
        await wrapper.setProps({
            isTree: true,
            data: mockupTreeData(wrapper),
        })
        expect(wrapper.vm.$props.isTree).to.equal(true)
    })
    it(`Tree data table:
      - Should not expand all nodes in first render`, async () => {
        await wrapper.setProps({
            isTree: true,
            data: mockupTreeData(wrapper),
        })
        expect(wrapper.vm.$data.hasValidChild).to.equal(true)
        expect(wrapper.vm.tableRows.length).to.equal(1)
        expect(wrapper.vm.tableRows[0].expanded).to.equal(false) // root_node is collapsed
        expect(wrapper.vm.tableRows[0].id).to.equal('root_node') //  only root_node
    })
    it(`Tree data table:
      - Should expand all nodes when editableCell is true`, async () => {
        await wrapper.setProps({
            isTree: true,
            data: mockupTreeData(wrapper),
        })

        /* ---------------  expand all nodes------------------------ */
        await wrapper.setProps({
            editableCell: true, // expand all nodes ( expandAllNodes function will be called)
        })
        expect(wrapper.vm.tableRows.length).to.equal(5)
        // tableRows should now includes all tree levels i.e. 0, 1, 2 and 3
        let allNodeIds = [
            'root_node',
            'node_child',
            'grand_child',
            'great_grand_child',
            'node_child_1',
        ]
        for (let i = 0; i < wrapper.vm.tableRows.length; ++i) {
            expect(wrapper.vm.tableRows[i].id).to.equal(allNodeIds[i])
        }
    })
    it(`Tree data table:
      - The collapse and expansion of node works as expected`, async () => {
        await wrapper.setProps({
            isTree: true,
            data: mockupTreeData(wrapper),
        })
        /* ---------------  expand root_node ------------------------ */
        wrapper.vm.toggleNode(wrapper.vm.tableRows[0]) //expand
        expect(wrapper.vm.tableRows[0].expanded).to.equal(true) // root_node is expanded
        expect(wrapper.vm.tableRows.length).to.equal(3)
        // tableRows should now includes tree levels i.e. 0 and 1
        let nodeIds_level_1 = ['root_node', 'node_child', 'node_child_1']
        for (let i = 0; i < wrapper.vm.tableRows.length; ++i) {
            expect(wrapper.vm.tableRows[i].id).to.equal(nodeIds_level_1[i])
        }

        /* ---------------  expand node_child ------------------------ */

        wrapper.vm.toggleNode(wrapper.vm.tableRows[1]) //expand node_child
        expect(wrapper.vm.tableRows[1].expanded).to.equal(true) // node_child is expanded
        // tableRows should now includes tree levels i.e. 0, 1 and 2
        let nodeIds_level_2 = ['root_node', 'node_child', 'grand_child', 'node_child_1']
        for (let i = 0; i < wrapper.vm.tableRows.length; ++i) {
            expect(wrapper.vm.tableRows[i].id).to.equal(nodeIds_level_2[i])
        }

        /* ---------------  Collapse root_node and its child ------------------------ */

        wrapper.vm.toggleNode(wrapper.vm.tableRows[0]) // collapse
        expect(wrapper.vm.tableRows[0].expanded).to.equal(false) // root_node is collapsed
        expect(wrapper.vm.tableRows.length).to.equal(1)
        expect(wrapper.vm.tableRows[0].id).to.equal('root_node') // only root_node
    })

    it(`Rowspan data table:
      - Should set background color of cells have same groupId to #fafcfc if
        a cell or a rowspan cell (rowspanCell) is hovered`, async () => {
        await wrapper.setProps({
            colsHasRowSpan: 2,
            headers: rowspanTableHeaders,
            data: rowspanTableData,
        })

        // get all table-cell components with ref = MonitorRowspanCell
        const rowspanCells = wrapper.findAllComponents({ ref: 'MonitorRowspanCell' })
        expect(rowspanCells.length).to.equal(4)
        /*
            trigger mouseenter at cell 0 having header as groupId and value equal to 'Monitor'
            cell-hover event should be emitted, setRowspanBg is called when
            colsHasRowSpan > 0
        */
        await rowspanCells.at(0).trigger('mouseenter')

        // testing for setRowspanBg method
        const normalCell = wrapper.findAllComponents({ ref: 'MonitorCell' })
        expect(normalCell.length).to.equal(14) // 7 columns with 2 rows have same groupId
        for (let i = 0; i < normalCell.length; ++i) {
            expect(normalCell.at(i).vm.$el.style['_values']['background-color']).to.equal(
                'rgb(250, 252, 252)'
            )
        }
    })

    it(`Rowspan data table:
      - Should emit cell-hover event and return accurate data when hovering a cell`, async () => {
        await wrapper.setProps({
            colsHasRowSpan: 2,
            headers: rowspanTableHeaders,
            data: rowspanTableData,
        })

        // add cb for cell-hover event
        let eventFired = 0
        wrapper.vm.$on('cell-hover', ({ item }) => {
            eventFired++
            expect(item.groupId).to.equal('Monitor')
        })

        // get all table-cell components with ref = MonitorRowspanCell
        const rowspanCells = wrapper.findAllComponents({ ref: 'MonitorRowspanCell' })
        expect(rowspanCells.length).to.equal(4)
        /*
            trigger mouseenter at cell 0 having header as groupId and value equal to 'Monitor'
            cell-hover event should be emitted, setRowspanBg is called when
            colsHasRowSpan > 0
        */
        await rowspanCells.at(0).trigger('mouseenter')

        /*  after hovering 'Monitor' cell, cb and cell-hover event have been triggered
            cell-hover event is emitted correctly
        */
        expect(eventFired).to.equal(1)
    })
})
